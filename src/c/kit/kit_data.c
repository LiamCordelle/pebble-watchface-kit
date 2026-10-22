#include "kit.h"
#include <string.h>
#include <stdio.h>

void kit_world_fill(LabData *d,int zone) {
  time_t stamp=d->epoch+kit_world_offset(zone,d->epoch)*60;
  struct tm world=*gmtime(&stamp);
  snprintf(d->world_label,sizeof(d->world_label),"%s",kit_world_label(zone));
  d->world_hour=world.tm_hour;d->world_min=world.tm_min;
  int32_t local_day=kit_epoch(d->now.tm_year+1900,d->now.tm_mon+1,d->now.tm_mday,0);
  int32_t world_day=kit_epoch(world.tm_year+1900,world.tm_mon+1,world.tm_mday,0);
  d->world_day_delta=(world_day-local_day)/86400;
}

void kit_moon_fill(LabData *d) {
  d->moon_phase_e3=kit_moon_phase_e3(d->epoch);
  d->moon_illum_pct=(int)(((int64_t)(TRIG_MAX_RATIO-cos_lookup(kit_moon_angle(d->epoch)))*50)/TRIG_MAX_RATIO);
}
#ifndef LAB_HOST_RENDER
void kit_live_data_refresh(LabData *d) {
  memset(d,0,sizeof(*d));
  d->epoch=time(NULL);
  d->now=*localtime(&d->epoch);
  d->minute_of_day=d->now.tm_hour*60+d->now.tm_min;
  BatteryChargeState battery=battery_state_service_peek();
  d->battery_pct=battery.charge_percent;d->charging=battery.is_charging;
  d->bt_connected=connection_service_peek_pebble_app_connection();
  kit_fill(d);
  kit_moon_fill(d);
}
#endif
