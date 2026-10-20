#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define KIT_VERSION 1
#define KIT_WEATHER_BYTES 204
#define KIT_WEATHER_TTL 10800
#define KIT_FORECAST_HOURS 48

// Pure, SDK-independent protocol, calendar and scheduling code.
typedef struct {
  int32_t observed, hourly_start, date[2], sunrise[2], sunset[2];
  int16_t temp, uv_x10, humidity, wind_kph, wind_from;
  int16_t high[2], low[2], uv_max[2];
  uint8_t code, count;
  int8_t hourly_temp[48];
  uint8_t hourly_rain[48], hourly_code[48];
} KitWeather;

typedef struct {
  bool enabled, ready, active;
  uint8_t retry;
  uint32_t id;
  int32_t due, deadline;
} KitSchedule;

uint32_t kit_checksum(const uint8_t *bytes, size_t size);
bool kit_weather_decode(const uint8_t *bytes, size_t size, int32_t now, KitWeather *out);
bool kit_weather_fresh(const KitWeather *weather, int32_t now);
bool kit_wmo_valid(int code);
int32_t kit_epoch(int year, int month, int day, int hour);
int kit_date_key(int32_t epoch);
int kit_world_offset(int zone, int32_t now);
const char *kit_world_label(int zone);
void kit_schedule_init(KitSchedule *s, bool enabled, int32_t now);
bool kit_schedule_due(const KitSchedule *s, int32_t now);
uint32_t kit_schedule_begin(KitSchedule *s, int32_t now);
void kit_schedule_fail(KitSchedule *s, int32_t now);
void kit_schedule_success(KitSchedule *s, int32_t now);
void kit_schedule_enable(KitSchedule *s, bool enabled, int32_t now);
// Compare with usual steps by this time; a zero baseline is unavailable.
void kit_format_pace(char *out, size_t size, int32_t steps, int32_t usual);
