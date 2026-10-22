#include "../src/c/kit/kit_calc.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  const int64_t anchor=947182440,period=2551443;
  assert(kit_moon_phase_e3(anchor)==0 && kit_moon_angle(anchor)==0);
  assert(kit_moon_phase_e3(anchor+period/2)==499);
  assert(kit_moon_phase_e3(anchor-1)==999);
  assert(kit_moon_phase_e3(anchor+period)==0);
  for (int64_t stamp=-10000000000LL;stamp<10000000000LL;stamp+=9876543) {
    int64_t age=(stamp-anchor)%period;if(age<0)age+=period;
    assert(kit_moon_phase_e3(stamp)==age*1000/period);
    assert(kit_moon_angle(stamp)==age*65536/period);
  }
  assert(kit_moon_phase_e3(INT64_MIN)>=0 && kit_moon_phase_e3(INT64_MAX)<1000);
  assert(kit_temperature(2,true)==36 && kit_temperature(-2,true)==28);
  assert(kit_temperature(-40,true)==-40 && kit_temperature(0,true)==32);
  assert(kit_temperature(2,false)==2 && kit_temperature(INT_MAX,true)==INT_MAX);
  char out[40];
  assert(kit_format_event_time(out,sizeof(out),0,false,1,KitMeridiemFull));assert(!strcmp(out,"12:00AM+1"));
  assert(kit_format_event_time(out,sizeof(out),720,false,-1,KitMeridiemCompact));assert(!strcmp(out,"12:00P-1"));
  assert(kit_format_event_time(out,sizeof(out),1439,true,0,KitMeridiemFull));assert(!strcmp(out,"23:59"));
  assert(!kit_format_event_time(out,sizeof(out),-1,true,0,KitMeridiemNone));assert(!strcmp(out,"--"));
  assert(!kit_format_event_time(out,2,720,false,0,KitMeridiemFull));
  assert(kit_daylight_state(true,0,0,600)==KitDaylightDay);
  assert(kit_daylight_state(true,600,0,600)==KitDaylightNight);
  assert(kit_daylight_state(true,360,360,1080)==KitDaylightDay);
  assert(kit_daylight_state(true,1080,360,1080)==KitDaylightNight);
  assert(kit_daylight_state(true,720,-1,-1)==KitDaylightUnknown);
  assert(kit_daylight_state(false,720,360,1080)==KitDaylightUnknown);
  assert(kit_daylight_state(true,720,1080,360)==KitDaylightUnknown);
  uint8_t rain[24]={20,40,80,80,255};
  uint8_t hours[24]={23,0,1,1,3},days[24]={0,1,1,1,1};
  assert(kit_forecast_rain_next(rain,5,1,50)==2);
  assert(kit_forecast_rain_peak(rain,5,0,30)==2);
  assert(kit_forecast_rain_next(rain,2,1,50)==-1);
  assert(kit_forecast_rain_peak(rain,255,0,0)==2); // read only 24 slots
  assert(kit_forecast_rain_next(NULL,5,0,0)==-1);
  assert(kit_forecast_hour_index(hours,days,5,0,1)==1);
  assert(kit_forecast_hour_index(hours,days,5,1,1)==2); // repeated DST hour
  assert(kit_forecast_hour_index(hours,days,5,2,1)==-1); // skipped DST hour
  assert(kit_forecast_hour_index(hours,days,5,0,0)==-1); // do not wrap days
  assert(kit_forecast_hour_index(hours,days,5,23,KIT_ANY_DAY)==0);
  uint16_t steps[24]={0};steps[6]=10000;
  assert(kit_goal_reached_minute(true,steps,370,5000)==365);
  assert(kit_goal_reached_minute(true,steps,370,10000)==370);
  steps[5]=5000;
  assert(kit_goal_reached_minute(true,steps,370,5000)==360);
  assert(kit_goal_reached_minute(true,steps,370,20000)==-1);
  assert(kit_goal_reached_minute(false,steps,370,5000)==-1);
  memset(steps,0,sizeof(steps));steps[0]=1;
  assert(kit_goal_eta_minute(true,1,steps,1439,50000)==71950000); // 64-bit product
  assert(kit_goal_eta_minute(true,8000,steps,370,8000)==370);
  assert(kit_goal_eta_minute(false,1,steps,370,8000)==-1);
  uint16_t history[28]={8000,9000,7000,10000};
  assert(kit_goal_days(history,28,8000)==3);
  assert(kit_goal_days(history,100,8000)==3);
  puts("Calculation tests passed: moon parity, rounding, event clocks, daylight unknown/boundaries, bounded/DST forecasts and activity overflow.");
}
