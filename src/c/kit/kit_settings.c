#include "kit.h"
#ifndef LAB_HOST_RENDER
#define SETTINGS_KEY 1000

static KitSettings s_settings = {KIT_VERSION,0,0,0,10000,1,0,0,0};

static bool valid(const KitSettings *s) {
  return s->version == KIT_VERSION && s->time_mode <= 2 && s->temp_unit <= 1 &&
      s->world <= 6 && s->step_goal >= 1000 && s->step_goal <= 50000 &&
      s->weather_enabled <= 1 && s->demo_fallback <= 1 && s->large_labels <= 1;
}

bool kit_tuple_int(DictionaryIterator *iter, uint32_t key, int32_t *value) {
  Tuple *t = dict_find(iter, key);
  if (!t || (t->type != TUPLE_INT && t->type != TUPLE_UINT)) return false;
  bool sign = t->type == TUPLE_INT;
  switch (t->length) {
    case 1: *value = sign ? t->value->int8 : t->value->uint8; return true;
    case 2: *value = sign ? t->value->int16 : t->value->uint16; return true;
    case 4: *value = t->value->int32; return true;
    default: return false;
  }
}

const KitSettings *kit_settings_get(void) { return &s_settings; }

void kit_settings_init(void) {
  KitSettings saved;
  if (persist_get_size(SETTINGS_KEY) == sizeof(saved) &&
      persist_read_data(SETTINGS_KEY, &saved, sizeof(saved)) == sizeof(saved) && valid(&saved)) {
    s_settings = saved;
  }
}

bool kit_settings_receive(DictionaryIterator *iter) {
  const uint32_t keys[] = {MESSAGE_KEY_TIME_MODE, MESSAGE_KEY_TEMP_UNIT,
    MESSAGE_KEY_STEP_GOAL, MESSAGE_KEY_WORLD, MESSAGE_KEY_WEATHER_ENABLED, MESSAGE_KEY_DEMO_FALLBACK,
    MESSAGE_KEY_LARGE_LABELS};
  int32_t v[7];
  for (int i = 0; i < 7; ++i) if (!kit_tuple_int(iter, keys[i], &v[i])) return false;
  if (v[0] < 0 || v[0] > 2 || v[1] < 0 || v[1] > 1 || v[2] < 1000 || v[2] > 50000 ||
      v[3] < 0 || v[3] > 6 || v[4] < 0 || v[4] > 1 || v[5] < 0 || v[5] > 1 ||
      v[6] < 0 || v[6] > 1) return false;
  KitSettings next = {KIT_VERSION,v[0],v[1],v[3],v[2],v[4],v[5],v[6],0};
  if (memcmp(&s_settings, &next, sizeof(next)) == 0) return false;
  s_settings = next;
  if (persist_write_data(SETTINGS_KEY, &next, sizeof(next)) != sizeof(next)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Kit settings could not be saved");
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Kit settings applied: time=%u unit=%u goal=%ld world=%u weather=%u demo=%u",
      next.time_mode,next.temp_unit,(long)next.step_goal,next.world,next.weather_enabled,next.demo_fallback);
  return true;
}

void kit_settings_apply(LabData *d) {
  d->step_goal = s_settings.step_goal;
  d->is_24h = s_settings.time_mode ? s_settings.time_mode == 2 : clock_is_24h_style();
  d->temp_fahrenheit = s_settings.temp_unit == 1;
  d->large_labels = s_settings.large_labels;
  time_t world = d->epoch + kit_world_offset(s_settings.world, d->epoch) * 60;
  struct tm t = *gmtime(&world);
  snprintf(d->world_label, sizeof(d->world_label), "%s", kit_world_label(s_settings.world));
  d->world_hour = t.tm_hour; d->world_min = t.tm_min;
  int32_t local_day = kit_epoch(d->now.tm_year+1900,d->now.tm_mon+1,d->now.tm_mday,0);
  int32_t world_day = kit_epoch(t.tm_year+1900,t.tm_mon+1,t.tm_mday,0);
  d->world_day_delta = (world_day - local_day) / 86400;
}
#endif
