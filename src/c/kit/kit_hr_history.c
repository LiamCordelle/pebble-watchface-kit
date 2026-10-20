#include "kit.h"
#ifndef LAB_HOST_RENDER
#if defined(PBL_HEALTH)
// Read flash in small batches, yielding between calls. Never allocate a day of
// minute records, change the sensor sampling period, or redraw from this timer.
#define HISTORY_BATCH 60
static time_t s_day=-1, s_cursor, s_target, s_last_now, s_scan_hour;
static int s_min, s_max;
static uint32_t s_sum[24];
static uint16_t s_count[24];
static bool s_partial;
static AppTimer *s_timer;
static void read_batch(void *context);

void kit_hr_history_clear(void) {
  if(s_timer)app_timer_cancel(s_timer);
  s_timer=NULL;s_day=-1;s_cursor=s_target=s_last_now=0;
  s_min=s_max=0;s_partial=false;
  memset(s_sum,0,sizeof(s_sum));memset(s_count,0,sizeof(s_count));
}
static void arm(void) {
  if(!s_timer && s_cursor<s_target)
    s_timer=app_timer_register(25,read_batch,NULL);
}
static void read_batch(void *context) {
  s_timer=NULL;
  if(s_day<0 || s_cursor>=s_target)return;
  const time_t requested=s_cursor;
  const time_t end=s_target-requested>HISTORY_BATCH*60 ? requested+HISTORY_BATCH*60 : s_target;
  time_t from=requested,to=end;
  HealthMinuteData records[HISTORY_BATCH];
  uint32_t count=health_service_get_minute_history(records,HISTORY_BATCH,&from,&to);
  // A zero result can mean missing history or a read failure. Advance one
  // bounded window, flag the coverage, and retry the day on an hourly refresh.
  if(!count || count>HISTORY_BATCH || from<requested || from>=end || from%60) {
    s_partial=true;s_cursor=end;
  } else {
    if(from>requested)s_partial=true;
    uint32_t used=0;
    for(uint32_t i=0;i<count;i++) {
      time_t stamp=from+i*60;
      if(stamp>=end)break;
      used++;
      if(records[i].is_invalid) {s_partial=true;continue;}
      int bpm=records[i].heart_rate_bpm;
      // Zero means no HR sample; an all-ones record is also never a BPM.
      if(bpm<=0 || bpm==UINT8_MAX)continue;
      if(!s_min || bpm<s_min)s_min=bpm;
      if(bpm>s_max)s_max=bpm;
      int hour=localtime(&stamp)->tm_hour;
      s_sum[hour]+=bpm;s_count[hour]++;
    }
    // Short reads may stop at a gap. Continue from the returned end instead of
    // throwing away the rest of that hour. Never trust mutable API end times.
    s_cursor=from+used*60;
    if(s_cursor<=requested) {s_partial=true;s_cursor=end;}
  }
  arm();
  if(s_cursor>=s_target)
    APP_LOG(APP_LOG_LEVEL_DEBUG,"HR minute history ready: data=%d partial=%d",s_min>0,s_partial);
}
void kit_hr_history_update(time_t day,time_t now,bool invalidate) {
  bool retry=s_partial && !s_timer && s_cursor>=s_target && now/3600!=s_scan_hour;
  if(day!=s_day || now<s_last_now || invalidate || retry) {
    kit_hr_history_clear();s_day=day;s_cursor=day;s_scan_hour=now/3600;
  }
  s_last_now=now;
  // Completed minutes are stable. The in-progress minute is read next tick,
  // rather than repeatedly counting a changing median as different samples.
  s_target=now-now%60;
  arm();
}
void kit_hr_history_fill(LabData *d) {
  d->hr_min=s_min;d->hr_max=s_max;
  d->hr_range_partial=s_partial || s_cursor<s_target;
  for(int h=0;h<24;h++)
    d->hr_by_hour[h]=s_count[h] ? (s_sum[h]+s_count[h]/2)/s_count[h] : 0;
}
#else
void kit_hr_history_clear(void) {}
void kit_hr_history_update(time_t day,time_t now,bool invalidate) {}
void kit_hr_history_fill(LabData *d) {
  d->hr_min=d->hr_max=0;d->hr_range_partial=false;
  memset(d->hr_by_hour,0,sizeof(d->hr_by_hour));
}
#endif
#endif
