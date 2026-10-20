#include "kit_core.h"
#include <string.h>
#include <stdio.h>

void kit_format_pace(char *out, size_t size, int32_t steps, int32_t usual) {
  if (usual <= 0) { snprintf(out,size,"No usual pace data"); return; }
  int64_t delta=(int64_t)steps-usual;
  if (!delta) { snprintf(out,size,"At usual pace"); return; }
  uint32_t n=(uint32_t)(delta<0?-delta:delta);
  char count[20];
  if(n>=1000000)snprintf(count,sizeof(count),"%lu,%03lu,%03lu",
      (unsigned long)(n/1000000),(unsigned long)((n/1000)%1000),(unsigned long)(n%1000));
  else if(n>=1000)snprintf(count,sizeof(count),"%lu,%03lu",(unsigned long)(n/1000),(unsigned long)(n%1000));
  else snprintf(count,sizeof(count),"%lu",(unsigned long)n);
  snprintf(out,size,"%s %s",count,delta>0?"ahead of usual":"behind usual");
}

static uint16_t u16(const uint8_t *p) { return p[0] | ((uint16_t)p[1] << 8); }
static uint32_t u32(const uint8_t *p) {
  return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

uint32_t kit_checksum(const uint8_t *p, size_t n) {
  uint32_t h = 2166136261u;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}

bool kit_wmo_valid(int c) {
  return (c >= 0 && c <= 3) || c == 45 || c == 48 || c == 51 || c == 53 ||
      c == 55 || c == 56 || c == 57 || c == 61 || c == 63 || c == 65 || c == 66 ||
      c == 67 || c == 71 || c == 73 || c == 75 || c == 77 || c == 80 || c == 81 ||
      c == 82 || c == 85 || c == 86 || c == 95 || c == 96 || c == 99;
}

bool kit_weather_fresh(const KitWeather *w, int32_t now) {
  return w->observed > 0 && w->observed <= now + 300 &&
      (int64_t)now - w->observed <= KIT_WEATHER_TTL;
}

bool kit_weather_decode(const uint8_t *p, size_t n, int32_t now, KitWeather *out) {
  if (!p || !out || n != KIT_WEATHER_BYTES || p[0] != 'M' || p[1] != 'K' ||
      p[2] != KIT_VERSION || p[3] < 24 || p[3] > 48 ||
      kit_checksum(p, 200) != u32(p + 200)) return false;
  KitWeather w = {0};
  w.count = p[3]; w.observed = u32(p + 4); w.hourly_start = u32(p + 8);
  w.date[0] = u32(p + 12); w.date[1] = u32(p + 16);
  w.sunrise[0] = u32(p + 20); w.sunset[0] = u32(p + 24);
  w.sunrise[1] = u32(p + 28); w.sunset[1] = u32(p + 32);
  w.temp = (int8_t)p[36]; w.code = p[37]; w.humidity = p[38];
  w.uv_x10 = u16(p + 39); w.wind_kph = u16(p + 41); w.wind_from = u16(p + 43);
  w.high[0] = (int8_t)p[45]; w.low[0] = (int8_t)p[46];
  w.high[1] = (int8_t)p[47]; w.low[1] = (int8_t)p[48];
  w.uv_max[0] = u16(p + 49); w.uv_max[1] = u16(p + 51);
  memcpy(w.hourly_temp, p + 53, 48);
  memcpy(w.hourly_rain, p + 101, 48);
  memcpy(w.hourly_code, p + 149, 48);
  if (!kit_weather_fresh(&w, now) || w.hourly_start < 1577836800 ||
      w.hourly_start > w.observed ||
      (int64_t)w.observed - w.hourly_start >= w.count * 3600 ||
      w.temp < -90 || w.temp > 65 || !kit_wmo_valid(w.code) || w.humidity > 100 ||
      w.uv_x10 > 300 || w.wind_kph > 500 || w.wind_from > 360) return false;
  for (int i = 0; i < 2; ++i) {
    if (w.date[i] < 20200101 || w.date[i] > 20380119 || w.high[i] < w.low[i] ||
        w.low[i] < -90 || w.high[i] > 65 || w.uv_max[i] > 300) return false;
    int y = w.date[i] / 10000, m = w.date[i] / 100 % 100, d = w.date[i] % 100;
    if (m < 1 || m > 12 || d < 1 || d > 31 || kit_date_key(kit_epoch(y,m,d,0)) != w.date[i]) return false;
    if (i && kit_epoch(y,m,d,0) - kit_epoch(w.date[0]/10000,w.date[0]/100%100,w.date[0]%100,0) != 86400) return false;
    // Polar days/nights are represented by an explicit pair of zero sun times.
    if ((w.sunrise[i] == 0) != (w.sunset[i] == 0)) return false;
    if (w.sunrise[i] && (w.sunrise[i] >= w.sunset[i] ||
        (int64_t)w.sunrise[i] < w.hourly_start - 86400 ||
        (int64_t)w.sunset[i] > (int64_t)w.hourly_start + 3 * 86400)) return false;
  }
  for (int h = 0; h < w.count; ++h) {
    if (w.hourly_temp[h] < -90 || w.hourly_temp[h] > 65 ||
        w.hourly_rain[h] > 100 || !kit_wmo_valid(w.hourly_code[h])) return false;
  }
  *out = w; // A malformed payload never changes the caller's valid snapshot.
  return true;
}

// Gregorian days since 1970, using integer civil-calendar arithmetic.
int32_t kit_epoch(int y, int m, int d, int h) {
  y -= m <= 2;
  int era = (y >= 0 ? y : y - 399) / 400;
  unsigned yo = (unsigned)(y - era * 400);
  unsigned dy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
  unsigned de = yo * 365 + yo / 4 - yo / 100 + dy;
  return (int32_t)(((int64_t)era * 146097 + de - 719468) * 86400 + h * 3600);
}

int kit_date_key(int32_t epoch) {
  int z = epoch / 86400 + 719468;
  int era = (z >= 0 ? z : z - 146096) / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  int y = (int)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  int day = doy - (153 * mp + 2) / 5 + 1;
  int month = mp + (mp < 10 ? 3 : -9);
  y += month <= 2;
  return y * 10000 + month * 100 + day;
}

static int sunday(int y, int m, int nth) {
  int first = (kit_epoch(y,m,1,0) / 86400 + 4) % 7;
  if (nth > 0) return 1 + (7 - first) % 7 + (nth - 1) * 7;
  int next = m == 12 ? kit_epoch(y+1,1,1,0) : kit_epoch(y,m+1,1,0);
  int days = (next - kit_epoch(y,m,1,0)) / 86400;
  return days - (first + days - 1) % 7;
}

const char *kit_world_label(int z) {
  static const char *const names[] = {"NZ", "UTC", "LON", "NYC", "LA", "TKY", "SYD"};
  return names[z >= 0 && z < 7 ? z : 0];
}

int kit_world_offset(int z, int32_t now) {
  static const int offsets[] = {720,0,0,-300,-480,540,600};
  if (z < 0 || z > 6) z = 0;
  int offset = offsets[z], year = kit_date_key(now) / 10000;
  int32_t start = 0, end = 0;
  if (z == 0 || z == 6) {
    int m = z == 0 ? 9 : 10;
    start = kit_epoch(year,m,sunday(year,m,z == 0 ? -1 : 1),2) - offset * 60;
    end = kit_epoch(year,4,sunday(year,4,1),2) - offset * 60;
    return offset + ((now < end || now >= start) ? 60 : 0);
  }
  if (z == 2) {
    start = kit_epoch(year,3,sunday(year,3,-1),1);
    end = kit_epoch(year,10,sunday(year,10,-1),1);
  } else if (z == 3 || z == 4) {
    start = kit_epoch(year,3,sunday(year,3,2),2) - offset * 60;
    end = kit_epoch(year,11,sunday(year,11,1),2) - (offset + 60) * 60;
  }
  return offset + (start && now >= start && now < end ? 60 : 0);
}

void kit_schedule_init(KitSchedule *s, bool enabled, int32_t now) {
  *s = (KitSchedule){.enabled=enabled, .due=now, .id=(uint32_t)now};
}
bool kit_schedule_due(const KitSchedule *s, int32_t now) {
  return s->enabled && s->ready && !s->active && now >= s->due;
}
uint32_t kit_schedule_begin(KitSchedule *s, int32_t now) {
  s->active = true; s->deadline = now + 45;
  s->id = (s->id + 1) & 0x7fffffff;
  if (!s->id) s->id = 1;
  return s->id;
}
void kit_schedule_fail(KitSchedule *s, int32_t now) {
  static const int retry[] = {60,300,900};
  s->active = false;
  if (s->retry < 3) s->due = now + retry[s->retry++];
  else { s->due = now + 1800; s->retry = 0; }
}
void kit_schedule_success(KitSchedule *s, int32_t now) {
  s->active = false; s->retry = 0; s->due = now + 1800;
}
void kit_schedule_enable(KitSchedule *s, bool enabled, int32_t now) {
  if (s->enabled == enabled) return;
  s->enabled = enabled; s->active = false; s->retry = 0; s->due = now;
}
