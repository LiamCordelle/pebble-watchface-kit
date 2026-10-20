#include "kit.h"
#ifndef LAB_HOST_RENDER
#define WEATHER_KEY 1001
_Static_assert(KIT_WEATHER_BYTES <= 256, "weather must fit one atomic persist value");
static KitWeather s_weather;
static KitSchedule s_schedule;
static bool s_have_weather;
static AppTimer *s_timer;
static void (*s_changed)(void);
static void pump(void);

static void timer(void *context) { s_timer = NULL; pump(); }
static void arm(void) {
  if (s_timer) { app_timer_cancel(s_timer); s_timer = NULL; }
  if (!s_schedule.enabled) return;
  time_t now = time(NULL);
  int32_t target = !s_schedule.ready ? now + 5 :
      (s_schedule.active ? s_schedule.deadline : s_schedule.due);
  uint32_t wait = target > now ? (target - now) * 1000 : 1;
  s_timer = app_timer_register(wait, timer, NULL);
}
static void pump(void) {
  time_t now = time(NULL);
  if (!s_schedule.ready) s_schedule.ready = true; // five-second startup grace
  if (s_schedule.active && now >= s_schedule.deadline) {
    APP_LOG(APP_LOG_LEVEL_INFO, "Weather watchdog expired");
    kit_schedule_fail(&s_schedule, now);
  }
  if (kit_schedule_due(&s_schedule,now)) {
    uint32_t id = kit_schedule_begin(&s_schedule,now);
    DictionaryIterator *iter = NULL;
    AppMessageResult r = app_message_outbox_begin(&iter);
    if (r == APP_MSG_OK && iter) {
      dict_write_uint32(iter,MESSAGE_KEY_REQUEST_WEATHER,id);
      r = app_message_outbox_send();
    }
    if (r != APP_MSG_OK || !iter) kit_schedule_fail(&s_schedule,now);
    else APP_LOG(APP_LOG_LEVEL_INFO,"Weather request %lu",(unsigned long)id);
  }
  arm();
}
void kit_weather_init(void (*changed)(void)) {
  s_changed = changed;
  kit_schedule_init(&s_schedule,kit_settings_get()->weather_enabled,time(NULL));
  uint8_t bytes[KIT_WEATHER_BYTES];
  if (persist_get_size(WEATHER_KEY) == sizeof(bytes) &&
      persist_read_data(WEATHER_KEY,bytes,sizeof(bytes)) == sizeof(bytes)) {
    s_have_weather = kit_weather_decode(bytes,sizeof(bytes),time(NULL),&s_weather);
    if (!s_have_weather) persist_delete(WEATHER_KEY);
  }
  arm();
}
void kit_weather_deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  s_timer = NULL; s_changed = NULL;
}
void kit_weather_ready(void) {
  s_schedule.ready = true;
  // A phone relaunch does not restart a completed half-hour schedule.
  pump();
}
void kit_weather_enabled(bool enabled) {
  kit_schedule_enable(&s_schedule,enabled,time(NULL));
  if (!enabled) {
    s_have_weather = false;
    memset(&s_weather,0,sizeof(s_weather));
    persist_delete(WEATHER_KEY);
  }
  if (s_schedule.ready) pump(); else arm();
}
void kit_weather_failed(DictionaryIterator *iter) {
  int32_t id;
  if (s_schedule.active && kit_tuple_int(iter,MESSAGE_KEY_REQUEST_WEATHER,&id) &&
      (uint32_t)id == s_schedule.id) {
    kit_schedule_fail(&s_schedule,time(NULL)); arm();
  }
}
void kit_weather_receive(DictionaryIterator *iter) {
  int32_t id;
  if (!s_schedule.enabled || !s_schedule.active ||
      !kit_tuple_int(iter,MESSAGE_KEY_WEATHER_ID,&id) || (uint32_t)id != s_schedule.id) return;
  Tuple *payload = dict_find(iter,MESSAGE_KEY_WEATHER);
  KitWeather next;
  if (!payload || payload->type != TUPLE_BYTE_ARRAY ||
      !kit_weather_decode(payload->value->data,payload->length,time(NULL),&next)) {
    APP_LOG(APP_LOG_LEVEL_INFO,"Weather rejected/failed; retaining valid cache");
    kit_schedule_fail(&s_schedule,time(NULL)); arm(); return;
  }
  s_weather = next; s_have_weather = true;
  if (persist_write_data(WEATHER_KEY,payload->value->data,KIT_WEATHER_BYTES) != KIT_WEATHER_BYTES)
    APP_LOG(APP_LOG_LEVEL_WARNING,"Weather cache could not be saved");
  kit_schedule_success(&s_schedule,time(NULL)); arm();
  APP_LOG(APP_LOG_LEVEL_INFO,"Weather snapshot accepted (%u hours)",next.count);
  if (s_changed) s_changed();
}
void kit_weather_tick(void) {
  if (s_have_weather && !kit_weather_fresh(&s_weather,time(NULL))) {
    s_have_weather = false;
    memset(&s_weather,0,sizeof(s_weather));
    persist_delete(WEATHER_KEY);
  }
  // Timers own the exact watchdog/retry deadlines; minute ticks recover allocation failures.
  if (!s_timer && s_schedule.enabled) pump();
}
static int minute(int32_t epoch) {
  if (!epoch) return -1;
  time_t value = epoch;
  struct tm t = *localtime(&value);
  return t.tm_hour * 60 + t.tm_min;
}
static int date_key(struct tm t) { return (t.tm_year+1900)*10000+(t.tm_mon+1)*100+t.tm_mday; }
void kit_weather_fill(LabData *d) {
  // Always clear the whole weather projection before deciding whether a snapshot is usable.
  d->weather_ok = false; d->weather_code = -1;
  d->temp = d->temp_hi = d->temp_lo = d->rain_pct = d->uv_x10 = d->uv_max_x10 = 0;
  d->humidity_pct = d->wind_kph = d->wind_from_deg = 0;
  d->sunrise_min = d->sunset_min = d->tomorrow_sunrise_min = d->tomorrow_sunset_min = -1;
  d->next_sun_min = -1; d->next_sun_day_delta = 0; d->next_sun_is_set = false;
  d->rolling_count = 0;
  memset(d->temp_by_hour,0,sizeof(d->temp_by_hour));
  memset(d->rain_by_hour,0,sizeof(d->rain_by_hour));
  memset(d->rolling_temp,0,sizeof(d->rolling_temp));
  memset(d->rolling_rain,0,sizeof(d->rolling_rain));
  memset(d->rolling_code,0,sizeof(d->rolling_code));
  memset(d->rolling_hour,0,sizeof(d->rolling_hour));
  memset(d->rolling_day,0,sizeof(d->rolling_day));
  if (!s_schedule.enabled || !s_have_weather || !kit_weather_fresh(&s_weather,d->epoch)) return;
  KitWeather *w = &s_weather;
  int di = w->date[0] == date_key(d->now) ? 0 : (w->date[1] == date_key(d->now) ? 1 : -1);
  int current = (d->epoch - w->hourly_start) / 3600;
  if (di < 0 || current < 0 || current >= w->count) return;
  d->weather_ok = true;
  d->temp = w->temp; d->temp_hi = w->high[di]; d->temp_lo = w->low[di];
  d->weather_code = w->code; d->rain_pct = w->hourly_rain[current];
  d->uv_x10 = w->uv_x10; d->uv_max_x10 = w->uv_max[di];
  d->humidity_pct = w->humidity; d->wind_kph = w->wind_kph; d->wind_from_deg = w->wind_from;
  d->sunrise_min = minute(w->sunrise[di]); d->sunset_min = minute(w->sunset[di]);
  if (di == 0) { d->tomorrow_sunrise_min = minute(w->sunrise[1]); d->tomorrow_sunset_min = minute(w->sunset[1]); }
  int32_t next = 0;
  for (int day = 0; day < 2; ++day) for (int set = 0; set < 2; ++set) {
    int32_t sun = set ? w->sunset[day] : w->sunrise[day];
    if (sun > d->epoch && (!next || sun < next)) { next = sun; d->next_sun_is_set = set; }
  }
  if (next) {
    d->next_sun_min = minute(next);
    time_t value = next;
    struct tm t = *localtime(&value);
    d->next_sun_day_delta = (kit_epoch(t.tm_year+1900,t.tm_mon+1,t.tm_mday,0) -
        kit_epoch(d->now.tm_year+1900,d->now.tm_mon+1,d->now.tm_mday,0)) / 86400;
  }
  for (int h = 0; h < w->count; ++h) {
    time_t value = w->hourly_start + h * 3600;
    struct tm t = *localtime(&value);
    if (date_key(t) == date_key(d->now)) {
      d->temp_by_hour[t.tm_hour] = w->hourly_temp[h];
      d->rain_by_hour[t.tm_hour] = w->hourly_rain[h];
    }
  }
  d->rolling_count = w->count - current < 24 ? w->count - current : 24;
  for (int i = 0; i < d->rolling_count; ++i) {
    d->rolling_temp[i] = w->hourly_temp[current+i];
    d->rolling_rain[i] = w->hourly_rain[current+i];
    d->rolling_code[i] = w->hourly_code[current+i];
    time_t value=w->hourly_start+(current+i)*3600;
    struct tm local=*localtime(&value);
    d->rolling_hour[i]=local.tm_hour;
    d->rolling_day[i]=(kit_epoch(local.tm_year+1900,local.tm_mon+1,local.tm_mday,0)-
        kit_epoch(d->now.tm_year+1900,d->now.tm_mon+1,d->now.tm_mday,0))/86400;
  }
}
#endif
