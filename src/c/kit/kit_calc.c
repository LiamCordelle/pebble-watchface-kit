#include "kit_calc.h"
#include <limits.h>
#include <stdio.h>

#define MOON_ANCHOR INT64_C(947182440)
#define MOON_PERIOD INT64_C(2551443)
static int64_t moon_age(int64_t epoch) {
  int64_t age = (epoch % MOON_PERIOD - MOON_ANCHOR % MOON_PERIOD) % MOON_PERIOD;
  return age < 0 ? age + MOON_PERIOD : age;
}
int kit_moon_phase_e3(int64_t epoch) { return (int)(moon_age(epoch) * 1000 / MOON_PERIOD); }
int32_t kit_moon_angle(int64_t epoch) { return (int32_t)(moon_age(epoch) * 65536 / MOON_PERIOD); }
static int int_result(int64_t value) {
  return value > INT_MAX ? INT_MAX : value < INT_MIN ? INT_MIN : (int)value;
}
int kit_temperature(int celsius, bool fahrenheit) {
  if (!fahrenheit) return celsius;
  return int_result(((int64_t)celsius * 9 + (celsius >= 0 ? 2 : -2)) / 5 + 32);
}
bool kit_format_event_time(char *out, size_t size, int minute, bool h24,
                           int delta, KitMeridiem meridiem) {
  if (!out || !size) return false;
  if (minute < 0 || minute >= 1440) { snprintf(out,size,"--"); return false; }
  int hour = minute / 60;
  char suffix[16] = "";
  if (delta) snprintf(suffix,sizeof(suffix),"%+d",delta);
  const char *ampm = meridiem == KitMeridiemNone ? "" :
    meridiem == KitMeridiemCompact ? (hour < 12 ? "A" : "P") : (hour < 12 ? "AM" : "PM");
  int written = h24 ? snprintf(out,size,"%02d:%02d%s",hour,minute%60,suffix) :
    snprintf(out,size,"%d:%02d%s%s",hour%12 ? hour%12 : 12,minute%60,ampm,suffix);
  return written >= 0 && (size_t)written < size;
}
KitDaylight kit_daylight_state(bool available,int minute,int rise,int set) {
  if (!available || minute < 0 || minute >= 1440 || rise < 0 || rise >= 1440 ||
      set <= rise || set >= 1440) return KitDaylightUnknown;
  return minute >= rise && minute < set ? KitDaylightDay : KitDaylightNight;
}
static int forecast_count(int count) { return count < 0 ? 0 : count > KIT_ROLLING_HOURS ? KIT_ROLLING_HOURS : count; }
int kit_forecast_rain_next(const uint8_t *rain,int count,int start,int threshold) {
  if (!rain || threshold < 0 || threshold > 100) return -1;
  for (int i=start < 0 ? 0 : start;i<forecast_count(count);++i)
    if (rain[i] <= 100 && rain[i] >= threshold) return i;
  return -1;
}
int kit_forecast_rain_peak(const uint8_t *rain,int count,int start,int threshold) {
  if (!rain || threshold < 0 || threshold > 100) return -1;
  int best=-1;
  for (int i=start < 0 ? 0 : start;i<forecast_count(count);++i)
    if (rain[i] <= 100 && rain[i] >= threshold && (best < 0 || rain[i] > rain[best])) best=i;
  return best;
}
int kit_forecast_hour_index(const uint8_t *hours,const uint8_t *days,int count,int hour,int day) {
  if (!hours || hour < 0 || hour > 23 || (day != KIT_ANY_DAY && !days)) return -1;
  for (int i=0;i<forecast_count(count);++i)
    if (hours[i]==hour && (day==KIT_ANY_DAY || days[i]==day)) return i;
  return -1;
}
int kit_goal_reached_minute(bool available,const uint16_t *hours,int minute,int32_t goal) {
  if (!available || !hours || minute < 0 || minute >= 1440 || goal <= 0) return -1;
  int64_t cumulative=0;
  for (int h=0;h<=minute/60;++h) {
    int64_t steps=hours[h];
    if (steps > 0 && cumulative+steps >= goal) {
      int elapsed=h==minute/60 ? minute%60 : 60;
      return h*60+(int)(((int64_t)goal-cumulative)*elapsed/steps);
    }
    cumulative+=steps;
  }
  return -1;
}
int kit_goal_eta_minute(bool available,int32_t steps,const uint16_t *hours,int minute,int32_t goal) {
  if (!available || !hours || steps <= 0 || goal <= 0 || minute < 0 || minute >= 1440) return -1;
  if (steps >= goal) return minute;
  int first=0;
  for (int h=0;h<=minute/60;++h) if (hours[h]) { first=h*60; break; }
  int elapsed=minute-first;
  if (elapsed < 1) elapsed=1;
  return int_result(minute+((int64_t)goal-steps)*elapsed/steps);
}
int kit_goal_days(const uint16_t *days,int count,int32_t goal) {
  if (!days || goal <= 0) return 0;
  if (count > 28) count=28;
  int result=0;
  for (int i=0;i<count;++i) if (days[i]>=goal) ++result;
  return result;
}
