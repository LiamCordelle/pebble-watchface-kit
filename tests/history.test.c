#include "../src/c/kit/kit.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

struct AppTimer { AppTimerCallback cb;void *context;bool armed; };
static AppTimer s_timer;
static HealthMinuteData s_minutes[1500];
static time_t s_base,s_first,s_fail_start=-1;
static int s_calls;
static bool s_short,s_bad,s_no_timer;
AppTimer *app_timer_register(uint32_t ms,AppTimerCallback cb,void *context) {
  assert(ms==25 && !s_timer.armed);
  if(s_no_timer)return NULL;
  s_timer=(AppTimer){.cb=cb,.context=context,.armed=true};return &s_timer;
}
bool app_timer_cancel(AppTimer *timer) { assert(timer==&s_timer);s_timer.armed=false;return true; }
uint32_t health_service_get_minute_history(HealthMinuteData *out,uint32_t max,time_t *from,time_t *to) {
  s_calls++;assert(max==60);assert(*to-*from<=3600);
  if(s_bad) {*from-=60;return max+1;}
  if(*from==s_fail_start)return 0;
  if(*from<s_first)*from=s_first;
  int index=(*from-s_base)/60;
  if(index<0 || index>=1500)return 0;
  uint32_t n=(*to>*from)?(*to-*from)/60:0;
  if(n>max)n=max;
  if(s_short && n>7)n=7;
  if(n>(uint32_t)(1500-index))n=1500-index;
  for(uint32_t i=0;i<n;i++)out[i]=s_minutes[index+i];
  *to=*from+n*60;return n;
}
static void drain(void) {
  int budget=1600;
  while(s_timer.armed) {
    assert(--budget>0);
    AppTimer t=s_timer;s_timer.armed=false;t.cb(t.context);
  }
}
static LabData state(void) {LabData d={0};kit_hr_history_fill(&d);return d;}
static void reset(void) {
  kit_hr_history_clear();s_calls=0;s_short=s_bad=s_no_timer=false;s_fail_start=-1;
  s_base=kit_epoch(2026,10,20,0);s_first=s_base;
  memset(s_minutes,0,sizeof(s_minutes));
}
int main(void) {
  setenv("TZ","UTC",1);tzset();reset();
  // Morning extrema remain visible at noon, independently of current BPM and
  // the aggregate API's two-hour limit. Zero/invalid records are not extrema.
  s_minutes[15].heart_rate_bpm=41;s_minutes[300].heart_rate_bpm=180;
  s_minutes[480].heart_rate_bpm=54;s_minutes[481].heart_rate_bpm=66;
  s_minutes[500].heart_rate_bpm=255;
  s_minutes[501]=(HealthMinuteData){.is_invalid=true,.heart_rate_bpm=1};
  s_minutes[502].heart_rate_bpm=0;
  kit_hr_history_update(s_base,s_base+12*3600,false);
  assert(state().hr_range_partial);assert(s_calls==0); // async, never blocks fill
  drain();LabData d=state();
  assert(d.hr_min==41 && d.hr_max==180 && d.hr_by_hour[8]==60);
  assert(d.hr_range_partial && s_calls==12); // invalid minute flags missing coverage
  int calls=s_calls;kit_hr_history_update(s_base,s_base+12*3600+25,false);drain();
  assert(s_calls==calls);
  s_minutes[720].heart_rate_bpm=200;
  kit_hr_history_update(s_base,s_base+12*3600+60,false);drain();d=state();
  assert(d.hr_min==41 && d.hr_max==200 && s_calls==calls+1);
  // Significant sync or restart rereads the OS records, rather than relying on
  // having kept this face active while the readings were recorded.
  s_minutes[300].heart_rate_bpm=99;
  kit_hr_history_update(s_base,s_base+12*3600+60,true);drain();
  assert(state().hr_max==200);
  kit_hr_history_clear();assert(!s_timer.armed && state().hr_min==0);
  kit_hr_history_update(s_base,s_base+12*3600+60,false);drain();
  assert(state().hr_min==41 && state().hr_max==200);
  // A clean dataset has no coverage marker. Short API reads are followed to
  // their actual end; later records in the same hour must not be skipped.
  reset();s_short=true;s_minutes[90].heart_rate_bpm=222;
  kit_hr_history_update(s_base,s_base+2*3600,false);drain();
  assert(state().hr_min==222 && state().hr_max==222 && !state().hr_range_partial);
  // An early missing interval makes the remaining day's range explicitly partial.
  reset();s_first=s_base+3600;s_minutes[90].heart_rate_bpm=70;
  kit_hr_history_update(s_base,s_base+2*3600,false);drain();
  assert(state().hr_min==70 && state().hr_range_partial);
  // Read failures cannot erase valid records in other windows or loop forever.
  reset();s_fail_start=s_base;s_minutes[100].heart_rate_bpm=80;
  kit_hr_history_update(s_base,s_base+2*3600,false);drain();
  assert(state().hr_min==80 && state().hr_range_partial && s_calls==2);
  s_fail_start=-1;s_minutes[10].heart_rate_bpm=50;
  kit_hr_history_update(s_base,s_base+3*3600,false);drain();
  assert(state().hr_min==50 && !state().hr_range_partial); // hourly retry repairs gap
  reset();s_bad=true;kit_hr_history_update(s_base,s_base+3600,false);drain();
  assert(!state().hr_min && state().hr_range_partial && s_calls==1);
  // Timer allocation failure is recoverable on the next update.
  reset();s_no_timer=true;s_minutes[5].heart_rate_bpm=75;
  kit_hr_history_update(s_base,s_base+3600,false);assert(state().hr_range_partial);
  s_no_timer=false;kit_hr_history_update(s_base,s_base+3600+60,false);drain();
  assert(state().hr_min==75 && !state().hr_range_partial);
  // No data produces dashes (zero extrema), never an hourly-average substitute.
  reset();kit_hr_history_update(s_base,s_base+3600,false);drain();
  assert(!state().hr_min && !state().hr_max && !state().hr_range_partial);
  // A day change or clock rollback drops the previous day's/extant extrema.
  s_minutes[5].heart_rate_bpm=77;
  kit_hr_history_update(s_base,s_base+3600,true);drain();assert(state().hr_max==77);
  s_base+=86400;s_first=s_base;memset(s_minutes,0,sizeof(s_minutes));
  kit_hr_history_update(s_base,s_base+60,false);drain();assert(!state().hr_max);
  s_minutes[0].heart_rate_bpm=60;
  kit_hr_history_update(s_base,s_base+120,false);drain();assert(!state().hr_min);
  kit_hr_history_update(s_base,s_base+60,false);drain();assert(state().hr_min==60);
  // Local days can last 23 or 25 hours. Repeated clock hours combine; a skipped
  // hour stays empty. The cache uses elapsed minutes, not a hard-coded 24h array.
  setenv("TZ","Europe/London",1);tzset();
  const int months[]={3,10},days[]={29,25},lengths[]={23,25};
  for(int k=0;k<2;k++) {
    reset();s_base=kit_epoch(2026,months[k],days[k],0)-(k?3600:0);s_first=s_base;
    s_minutes[0].heart_rate_bpm=50;s_minutes[lengths[k]*60-1].heart_rate_bpm=190;
    if(k) {s_minutes[60].heart_rate_bpm=60;s_minutes[120].heart_rate_bpm=100;}
    kit_hr_history_update(s_base,s_base+lengths[k]*3600,false);drain();d=state();
    assert(d.hr_min==50 && d.hr_max==190 && !d.hr_range_partial);
    assert(s_calls==lengths[k]);
    if(k)assert(d.hr_by_hour[1]==80);else assert(!d.hr_by_hour[1]);
  }
  kit_hr_history_clear();
  puts("History tests passed: day reconstruction, true minute extrema, gaps, async batches, incremental cache, restart, permission clear, midnight and DST.");
}
