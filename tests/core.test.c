#include "../src/c/kit/kit_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void sign_payload(uint8_t *p) {
  uint32_t h=kit_checksum(p,200);
  for(int i=0;i<4;i++)p[200+i]=(h>>(i*8))&255;
}
int main(int argc,char **argv) {
  assert(argc==2);
  FILE *f=fopen(argv[1],"rb");assert(f);
  uint8_t bytes[KIT_WEATHER_BYTES],bad[KIT_WEATHER_BYTES];
  assert(fread(bytes,1,sizeof(bytes),f)==sizeof(bytes));fclose(f);
  int32_t now=kit_epoch(2026,10,17,9)+9*60;
  KitWeather w={0};assert(kit_weather_decode(bytes,sizeof(bytes),now,&w));
  assert(w.temp==-12 && w.hourly_temp[0]==-12 && w.uv_x10==34 && w.uv_max[0]==42);
  assert(w.date[0]==20261017 && w.date[1]==20261018 && w.hourly_rain[28]==80);
  assert(kit_weather_fresh(&w,w.observed+10800));
  assert(!kit_weather_fresh(&w,w.observed+10801));
  assert(!kit_weather_fresh(&w,w.observed-301));
  KitWeather old=w;
  assert(!kit_weather_decode(bytes,sizeof(bytes)-1,now,&w));assert(memcmp(&old,&w,sizeof(w))==0);
  memcpy(bad,bytes,sizeof(bad));bad[101]=101;sign_payload(bad);
  assert(!kit_weather_decode(bad,sizeof(bad),now,&w));assert(memcmp(&old,&w,sizeof(w))==0);
  memcpy(bad,bytes,sizeof(bad));bad[149]=4;sign_payload(bad);
  assert(!kit_weather_decode(bad,sizeof(bad),now,&w));
  memcpy(bad,bytes,sizeof(bad));bad[4]^=1;
  assert(!kit_weather_decode(bad,sizeof(bad),now,&w));
  memcpy(bad,bytes,sizeof(bad));bad[3]=23;sign_payload(bad);
  assert(!kit_weather_decode(bad,sizeof(bad),now,&w));
  assert(!kit_weather_decode(bytes,sizeof(bytes),now+10801,&w));
  assert(memcmp(&old,&w,sizeof(w))==0);

  KitSchedule s;kit_schedule_init(&s,true,1000);
  assert(!kit_schedule_due(&s,1000));s.ready=true;assert(kit_schedule_due(&s,1000));
  uint32_t id=kit_schedule_begin(&s,1000);assert(id>0 && s.deadline==1045 && !kit_schedule_due(&s,9999));
  kit_schedule_fail(&s,1045);assert(s.due==1105 && s.retry==1);
  assert(!kit_schedule_due(&s,1104));assert(kit_schedule_due(&s,1105));
  kit_schedule_begin(&s,1105);kit_schedule_fail(&s,1150);assert(s.due==1450);
  kit_schedule_begin(&s,1450);kit_schedule_fail(&s,1495);assert(s.due==2395);
  kit_schedule_begin(&s,2395);kit_schedule_fail(&s,2440);assert(s.due==4240 && s.retry==0);
  kit_schedule_begin(&s,4240);kit_schedule_success(&s,4241);assert(s.due==6041 && !s.active);
  kit_schedule_enable(&s,false,4242);assert(!kit_schedule_due(&s,10000));
  kit_schedule_enable(&s,true,5000);assert(kit_schedule_due(&s,5000));
  assert(kit_epoch(1970,1,1,0)==0);
  assert(kit_date_key(kit_epoch(2024,2,29,12))==20240229);
  const int zones[]={0,2,3,4,6};
  const int32_t starts[]={1790431200,1774746000,1772953200,1772964000,1791043200};
  const int32_t ends[]={1775311200,1792890000,1793512800,1793523600,1775318400};
  const int standard[]={720,0,-300,-480,600};
  for(int i=0;i<5;i++) {
    assert(kit_world_offset(zones[i],starts[i]-1)==standard[i]);
    assert(kit_world_offset(zones[i],starts[i])==standard[i]+60);
    assert(kit_world_offset(zones[i],ends[i]-1)==standard[i]+60);
    assert(kit_world_offset(zones[i],ends[i])==standard[i]);
  }
  char pace[64];
  kit_format_pace(pace,sizeof(pace),6842,5120);assert(!strcmp(pace,"1,722 ahead of usual"));
  kit_format_pace(pace,sizeof(pace),3000,5120);assert(!strcmp(pace,"2,120 behind usual"));
  kit_format_pace(pace,sizeof(pace),5120,5120);assert(!strcmp(pace,"At usual pace"));
  kit_format_pace(pace,sizeof(pace),6842,0);assert(!strcmp(pace,"No usual pace data"));
  puts("C core tests passed: cross-language protocol, atomic rejection, cache expiry, watchdog/retries and DST boundaries.");
}
