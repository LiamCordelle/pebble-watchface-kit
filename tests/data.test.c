#include "../src/c/kit/kit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static int reads;
static int32_t angle_seen;
time_t kit_test_time(time_t *out) {
  time_t now=947182440;
  if(out)*out=now;
  ++reads;return now;
}
int32_t cos_lookup(int32_t angle) { angle_seen=angle;return 32768; }
BatteryChargeState battery_state_service_peek(void) { return (BatteryChargeState){76,true}; }
bool connection_service_peek_pebble_app_connection(void) { return true; }
void kit_fill(LabData *d) {
  assert(d->epoch==947182440 && d->minute_of_day==18*60+14);
  assert(d->wind_kph==0 && d->steps==0 && d->world_label[0]==0);
  d->steps_ok=true;d->steps=6842;d->step_goal=8000;d->weather_ok=true;d->temp=14;
}
int main(void) {
  setenv("TZ","UTC",1);tzset();
  LabData d;memset(&d,255,sizeof(d));kit_live_data_refresh(&d);
  assert(reads==1 && d.battery_pct==76 && d.charging && d.bt_connected);
  assert(d.steps==6842 && d.step_goal==8000 && d.weather_ok && d.temp==14);
  assert(d.moon_phase_e3==0 && angle_seen==0 && d.moon_illum_pct==25);
  kit_world_fill(&d,1);assert(!strcmp(d.world_label,"UTC") && d.world_hour==18 && d.world_min==14 && d.world_day_delta==0);
  d.epoch=947182439;kit_moon_fill(&d);assert(d.moon_phase_e3==999 && angle_seen>65000);
  puts("Data adapter passed: single clock snapshot, cleared data, device/services projection and lookup-trig moon illumination.");
}
