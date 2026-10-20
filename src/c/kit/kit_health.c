#include "kit.h"
#ifndef LAB_HOST_RENDER
static LabData s_health;
static time_t s_last_minute = -1;
static time_t s_day = -1;
static time_t s_history_hour = -1;
static bool s_hr_access;
static bool s_hr_history_invalid;
static HealthServiceAccessibilityMask s_hr_mask;

#if defined(PBL_HEALTH)
static bool available(HealthMetric m, time_t from, time_t to) {
  return (health_service_metric_accessible(m,from,to) & HealthServiceAccessibilityMaskAvailable) != 0;
}
static int32_t sum(HealthMetric m, time_t from, time_t to) {
  return from < to && available(m,from,to) ? health_service_sum(m,from,to) : 0;
}
static int aggregate(HealthMetric m, time_t from, time_t to, HealthAggregation a,
                     HealthServiceTimeScope scope) {
  return (health_service_metric_aggregate_averaged_accessible(m,from,to,a,scope) &
      HealthServiceAccessibilityMaskAvailable) ? health_service_aggregate_averaged(m,from,to,a,scope) : 0;
}
static time_t local_boundary(struct tm t, int day_delta, int hour) {
  // Pebble mktime uses tm_gmtoff directly; setting tm_isdst=-1 does not resolve DST.
  int32_t wanted=kit_epoch(t.tm_year+1900,t.tm_mon+1,t.tm_mday,0)+day_delta*86400+hour*3600;
  time_t candidate=wanted, latest=candidate;
  for(int i=0;i<4;i++) {
    struct tm local=*localtime(&candidate);
    int32_t wall=kit_epoch(local.tm_year+1900,local.tm_mon+1,local.tm_mday,local.tm_hour)+
        local.tm_min*60+local.tm_sec;
    if(wall==wanted) {
      // For a repeated hour, use the first occurrence; its bucket contains both.
      time_t earlier=candidate-3600;
      struct tm e=*localtime(&earlier);
      int32_t early_wall=kit_epoch(e.tm_year+1900,e.tm_mon+1,e.tm_mday,e.tm_hour)+e.tm_min*60+e.tm_sec;
      return early_wall==wanted ? earlier : candidate;
    }
    candidate+=wanted-wall;
    if(candidate>latest || i==0)latest=candidate;
  }
  // A skipped local hour normalises forward, making the missing bucket empty.
  return latest;
}
static void refresh_heart_rate(time_t now) {
  // Match the instantaneous query used by peek_current_value (Signal Deck).
  // A midnight-to-now query can exceed PebbleOS's two-hour HR history limit,
  // even while a valid current filtered reading is available.
  s_hr_mask = health_service_metric_aggregate_averaged_accessible(
      HealthMetricHeartRateBPM,now,now,HealthAggregationAvg,
      HealthServiceTimeScopeOnce);
  bool access=(s_hr_mask & HealthServiceAccessibilityMaskAvailable)!=0;
  if(s_hr_mask & (HealthServiceAccessibilityMaskNoPermission | HealthServiceAccessibilityMaskNotSupported))
    kit_hr_history_clear();
  if (access != s_hr_access) {s_history_hour=-1;s_hr_access=access;}
  s_health.heart_rate = access ? health_service_peek_current_value(HealthMetricHeartRateBPM) : 0;
  s_health.hr_ok = access && s_health.heart_rate > 0;
}
static void refresh(const LabData *d);
static void event(HealthEventType type, void *context) {
  if (type == HealthEventSignificantUpdate) {
    s_day=-1;s_history_hour=-1;s_last_minute=-1;s_hr_history_invalid=true;
  }
  // History work is coalesced, but a later HR event in the same minute must
  // still capture the latest filtered value. Neither path redraws the canvas.
  time_t now=time(NULL);
  if(s_last_minute!=now/60 && (type==HealthEventMovementUpdate ||
      type==HealthEventHeartRateUpdate || type==HealthEventSleepUpdate ||
      type==HealthEventSignificantUpdate)) {
    LabData clock={0};clock.epoch=now;clock.now=*localtime(&now);
    refresh(&clock);s_last_minute=now/60;
  } else if (type==HealthEventHeartRateUpdate || type==HealthEventSignificantUpdate) {
    refresh_heart_rate(now);
  }
}
static void refresh(const LabData *d) {
  time_t now = d->epoch, day = local_boundary(d->now,0,0);
  bool steps_ok = available(HealthMetricStepCount,day,now);
  if (s_day != day || !steps_ok || !s_health.steps_ok) {
    s_history_hour=-1;
    memset(s_health.steps_by_day,0,sizeof(s_health.steps_by_day));
    if (steps_ok) for (int k = 1; k < 28; ++k) {
      time_t from = local_boundary(d->now,-k,0), to = local_boundary(d->now,1-k,0);
      s_health.steps_by_day[k] = lab_clamp(sum(HealthMetricStepCount,from,to),0,65535);
    }
    s_day = day;
  }
  s_health.steps_ok = steps_ok;
  s_health.steps = steps_ok ? sum(HealthMetricStepCount,day,now) : 0;
  s_health.steps_typical = steps_ok ? aggregate(HealthMetricStepCount,day,now,
      HealthAggregationSum,HealthServiceTimeScopeDailyWeekdayOrWeekend) : 0;
  s_health.steps_by_day[0] = lab_clamp(s_health.steps,0,65535);
  refresh_heart_rate(now);
  if(!(s_hr_mask & (HealthServiceAccessibilityMaskNoPermission | HealthServiceAccessibilityMaskNotSupported)))
    kit_hr_history_update(day,now,s_hr_history_invalid);
  s_hr_history_invalid=false;
  for (int h = 0; h < 24; ++h) {
    // Completed hours are stable until a new hour or significant health sync.
    if(s_history_hour==now/3600 && h!=d->now.tm_hour)continue;
    time_t from = local_boundary(d->now,0,h), to = local_boundary(d->now,0,h+1);
    if (to > now) to = now;
    s_health.steps_by_hour[h] = steps_ok && from < now ? lab_clamp(sum(HealthMetricStepCount,from,to),0,65535) : 0;
  }
  s_history_hour=now/3600;
  s_health.active_minutes = sum(HealthMetricActiveSeconds,day,now) / 60;
  s_health.distance_m = sum(HealthMetricWalkedDistanceMeters,day,now);
  s_health.active_kcal = sum(HealthMetricActiveKCalories,day,now);
  // Last night's sleep includes the portion before midnight.
  s_health.sleep_minutes = sum(HealthMetricSleepSeconds,local_boundary(d->now,-1,18),now) / 60;
}
#endif

void kit_health_init(void) {
#if defined(PBL_HEALTH)
  if (!health_service_events_subscribe(event,NULL)) APP_LOG(APP_LOG_LEVEL_INFO,"Health events unavailable");
#endif
}
void kit_health_deinit(void) {
  kit_hr_history_clear();
#if defined(PBL_HEALTH)
  health_service_events_unsubscribe();
#endif
}
void kit_health_fill(LabData *d) {
  if (s_last_minute != d->epoch / 60) {
#if defined(PBL_HEALTH)
    // Also refresh at a minute tick: permission changes may have no event.
    refresh(d);
#endif
    s_last_minute = d->epoch / 60;
  }
  d->steps_ok = s_health.steps_ok; d->steps = s_health.steps;
  d->steps_typical = s_health.steps_typical;
  d->hr_ok = s_health.hr_ok; d->heart_rate = s_health.heart_rate;
  kit_hr_history_fill(d);
  d->active_minutes = s_health.active_minutes; d->distance_m = s_health.distance_m;
  d->active_kcal = s_health.active_kcal; d->sleep_minutes = s_health.sleep_minutes;
  memcpy(d->steps_by_day,s_health.steps_by_day,sizeof(d->steps_by_day));
  memcpy(d->steps_by_hour,s_health.steps_by_hour,sizeof(d->steps_by_hour));
  if (kit_settings_get()->demo_fallback) {
    if (!d->steps_ok || !d->steps) {
      d->steps_ok = true; d->steps = 6842; d->steps_typical = 5120;
      static const uint16_t week[] = {6842,11230,9874,12640,8120,10450,4320};
      for (int k = 0; k < 28; ++k) d->steps_by_day[k] = week[k%7];
    }
    if (!d->hr_ok) {
      d->hr_ok=true;d->heart_rate=64;
      if(!d->hr_min || !d->hr_max) {
        d->hr_min=52;d->hr_max=141;d->hr_range_partial=false;
      }
    }
  }
}
#endif
