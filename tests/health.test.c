#include "../src/c/kit/kit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static time_t s_now;
static int s_bpm, s_history_reads, s_current_queries;
static bool s_permission=true;
static HealthEventHandler s_callback;
static void *s_context;
static KitSettings s_settings;
struct AppTimer { int unused; };
static AppTimer s_timer;
static bool s_history_pending;
AppTimer *app_timer_register(uint32_t ms,AppTimerCallback cb,void *context) {
  (void)ms;(void)cb;(void)context;assert(!s_history_pending);
  s_history_pending=true;return &s_timer;
}
bool app_timer_cancel(AppTimer *timer) {
  assert(timer==&s_timer);s_history_pending=false;return true;
}
uint32_t health_service_get_minute_history(HealthMinuteData *data,uint32_t count,time_t *a,time_t *b) {
  (void)data;(void)count;(void)a;(void)b;return 0;
}

time_t health_test_time(time_t *out) { if(out)*out=s_now;return s_now; }
const KitSettings *kit_settings_get(void) { return &s_settings; }
int lab_clamp(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
HealthServiceAccessibilityMask health_service_metric_accessible(HealthMetric m,time_t a,time_t b) {
  // Contemporary PebbleOS selects Avg by default for HR. Its history limit
  // still rejects midnight-to-now queries more than two hours after midnight.
  if(m==HealthMetricHeartRateBPM)
    return health_service_metric_aggregate_averaged_accessible(
        m,a,b,HealthAggregationAvg,HealthServiceTimeScopeOnce);
  return HealthServiceAccessibilityMaskAvailable;
}
HealthServiceAccessibilityMask health_service_metric_aggregate_averaged_accessible(
    HealthMetric m,time_t a,time_t b,HealthAggregation agg,HealthServiceTimeScope scope) {
  if(m!=HealthMetricHeartRateBPM)return HealthServiceAccessibilityMaskAvailable;
  if(!s_permission)return HealthServiceAccessibilityMaskNoPermission;
  if(a==s_now && b==s_now && agg==HealthAggregationAvg) {
    assert(scope==HealthServiceTimeScopeOnce);
    s_current_queries++;
  }
  if(s_now-a>2*3600)return HealthServiceAccessibilityMaskNotSupported;
  return HealthServiceAccessibilityMaskAvailable;
}
HealthValue health_service_sum(HealthMetric m,time_t a,time_t b) {
  (void)m;(void)a;(void)b;s_history_reads++;return 1234;
}
HealthValue health_service_aggregate_averaged(
    HealthMetric m,time_t a,time_t b,HealthAggregation agg,HealthServiceTimeScope scope) {
  (void)m;(void)a;(void)b;(void)agg;(void)scope;s_history_reads++;return 0;
}
HealthValue health_service_peek_current_value(HealthMetric m) {
  assert(m==HealthMetricHeartRateBPM);return s_bpm;
}
bool health_service_events_subscribe(HealthEventHandler cb,void *ctx) {
  s_callback=cb;s_context=ctx;return true;
}
void health_service_events_unsubscribe(void) { s_callback=NULL; }
static LabData fill(void) {
  LabData d={0};d.epoch=s_now;d.now=*localtime(&s_now);kit_health_fill(&d);return d;
}
static void emit(HealthEventType type) { assert(s_callback);s_callback(type,s_context); }
int main(void) {
  setenv("TZ","UTC",1);tzset();kit_health_init();
  const int hours[]={0,1,2,3,12,23};
  for(size_t i=0;i<sizeof(hours)/sizeof(hours[0]);i++) {
    s_now=kit_epoch(2026,10,19,hours[i])+30*60;s_bpm=72;
    LabData d=fill();assert(d.hr_ok && d.heart_rate==72 && d.steps_ok);
  }
  // Midnight rollover also works without relying on earlier day's data.
  s_now=kit_epoch(2026,10,20,0);s_bpm=68;
  LabData d=fill();assert(d.hr_ok && d.heart_rate==68);
  // No sample at the minute refresh, then multiple valid samples in that
  // same minute. Capture all events without repeating historical work.
  s_now=kit_epoch(2026,10,20,12);s_bpm=0;
  d=fill();assert(!d.hr_ok && d.heart_rate==0);
  int history=s_history_reads;
  s_now+=10;s_bpm=78;emit(HealthEventHeartRateUpdate);
  d=fill();assert(d.hr_ok && d.heart_rate==78 && s_history_reads==history);
  s_now+=10;s_bpm=81;emit(HealthEventHeartRateUpdate);
  d=fill();assert(d.hr_ok && d.heart_rate==81 && s_history_reads==history);
  // Permission loss must never leave a cached reading on screen.
  s_permission=false;emit(HealthEventSignificantUpdate);
  d=fill();assert(!d.hr_ok && d.heart_rate==0 && !d.hr_min && !d.hr_max);
  assert(!s_history_pending);
  s_permission=true;s_bpm=76;emit(HealthEventSignificantUpdate);
  d=fill();assert(d.hr_ok && d.heart_rate==76);
  // Let the OS expire its filtered value: never resurrect a raw/demo/stale BPM.
  s_now+=60;s_bpm=0;d=fill();assert(!d.hr_ok && d.heart_rate==0);
  assert(s_current_queries>0);
  kit_health_deinit();assert(!s_callback && !s_history_pending);
  puts("Health tests passed: all-day current HR, same-minute events, permission loss, expiry and bounded history work.");
}
