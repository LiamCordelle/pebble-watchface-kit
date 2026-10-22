#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Pure calculations: no Pebble SDK, resources, colours or drawing policy.
#define KIT_ROLLING_HOURS 24
#define KIT_ANY_DAY (-1)
typedef enum { KitDaylightUnknown, KitDaylightDay, KitDaylightNight } KitDaylight;
typedef enum { KitMeridiemNone, KitMeridiemCompact, KitMeridiemFull } KitMeridiem;

// Existing mean-synodic approximation, normalized for epochs before its anchor.
int kit_moon_phase_e3(int64_t epoch);
int32_t kit_moon_angle(int64_t epoch); // one cycle = 65536, for Pebble lookup trig
int kit_temperature(int celsius, bool fahrenheit);
// Invalid minutes produce "--" and false. day_delta is explicit, e.g. +1/-1.
bool kit_format_event_time(char *out, size_t size, int minute, bool is_24h,
                           int day_delta, KitMeridiem meridiem);
KitDaylight kit_daylight_state(bool available, int minute, int sunrise, int sunset);

// Counts are bounded by the LabData capacity. Earliest chronological tie wins.
int kit_forecast_rain_next(const uint8_t *rain, int count, int start, int threshold);
int kit_forecast_rain_peak(const uint8_t *rain, int count, int start, int threshold);
int kit_forecast_hour_index(const uint8_t *hours, const uint8_t *days, int count,
                            int hour, int day_delta);

// Estimated bucket interpolation/pace, not a recorded exact attainment time.
// -1 means unavailable. ETA may exceed midnight; consumers choose presentation.
int kit_goal_reached_minute(bool available, const uint16_t *hours, int minute, int32_t goal);
int kit_goal_eta_minute(bool available, int32_t steps, const uint16_t *hours,
                        int minute, int32_t goal);
// Includes today when present; this is a count, not a consecutive-day streak.
int kit_goal_days(const uint16_t *days, int count, int32_t goal);
