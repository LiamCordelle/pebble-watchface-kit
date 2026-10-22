#include "kit.h"
#ifndef LAB_HOST_RENDER
#ifndef KIT_TOUCH_WINDOW_MS
#define KIT_TOUCH_WINDOW_MS 0
#endif
#define DETAIL_TIMEOUT_MS 8000
static Window *s_window;
static Layer *s_canvas;
static const LabDesign *s_design;
static LabData s_data;
static bool s_detail;
static AppTimer *s_detail_timer;
static uint32_t now_ms(void) {
  time_t seconds; uint16_t millis;
  time_ms(&seconds,&millis);
  return (uint32_t)seconds * 1000 + millis;
}
static void refresh(void) {
  lab_data_refresh(&s_data);
  if (s_canvas) layer_mark_dirty(s_canvas);
}
static void paint(Layer *layer, GContext *ctx) {
  uint32_t start = now_ms();
  GRect bounds = layer_get_bounds(layer);
  if (s_detail && s_design->draw_detail) s_design->draw_detail(ctx,bounds,&s_data);
  else s_design->draw(ctx,bounds,&s_data);
  APP_LOG(APP_LOG_LEVEL_DEBUG,"Frame %s %lums",s_detail ? "detail" : "face",
      (unsigned long)(now_ms()-start));
}
static void detail_expired(void *context) {
  s_detail_timer = NULL; s_detail = false;
  if (s_canvas) layer_mark_dirty(s_canvas);
}
static void toggle_detail(void) {
  if (!s_design->draw_detail) return;
  if (s_detail_timer) {app_timer_cancel(s_detail_timer);s_detail_timer=NULL;}
  s_detail = !s_detail;
  if (s_detail) s_detail_timer = app_timer_register(DETAIL_TIMEOUT_MS,detail_expired,NULL);
  refresh();
}
#if defined(PBL_TOUCH)
static GPoint s_start,s_last;
static uint32_t s_touch_start;
static bool s_down,s_touch_subscribed;
#if KIT_TOUCH_WINDOW_MS > 0
static AppTimer *s_touch_timer;
static void touch_expired(void *context) {
  s_touch_timer=NULL;touch_service_unsubscribe();s_touch_subscribed=false;s_down=false;
}
#endif
static void touch(const TouchEvent *event, void *context) {
  GPoint point=GPoint(event->x,event->y);
  switch(event->type) {
    case TouchEvent_Touchdown:
      s_start=s_last=point;s_touch_start=now_ms();s_down=true;break;
    case TouchEvent_PositionUpdate:
      if(s_down)s_last=point;
      break;
    case TouchEvent_Liftoff:
      if(!s_down)break;
      s_down=false;
      if(point.x || point.y)s_last=point;
      if(abs(s_last.x-s_start.x)<=16 && abs(s_last.y-s_start.y)<=16 &&
          now_ms()-s_touch_start<=600)toggle_detail();
      break;
  }
}
static void touch_enable(void) {
  if(!s_touch_subscribed){touch_service_subscribe(touch,NULL);s_touch_subscribed=true;}
#if KIT_TOUCH_WINDOW_MS > 0
  if(s_touch_timer)app_timer_cancel(s_touch_timer);
  s_touch_timer=app_timer_register(KIT_TOUCH_WINDOW_MS,touch_expired,NULL);
#endif
}
#endif
static void flick(AccelAxisType axis,int32_t direction) {
#if defined(PBL_TOUCH)
  touch_enable();
#endif
  toggle_detail();
}
static void tick(struct tm *now,TimeUnits changed) {
#if LAB_SCENARIO == LAB_LIVE
  kit_tick();
#endif
  refresh();
}
static void battery(BatteryChargeState state) {
  refresh();
#if LAB_SCENARIO == LAB_LIVE
  // A peek can still contain its previous value inside the service callback.
  s_data.battery_pct=state.charge_percent;s_data.charging=state.is_charging;
#endif
}
static void connection(bool connected) {
  refresh();
#if LAB_SCENARIO == LAB_LIVE
  s_data.bt_connected=connected;
#endif
}
static void load(Window *window) {
  Layer *root=window_get_root_layer(window);
  s_canvas=layer_create(layer_get_bounds(root));
  if(!s_canvas){APP_LOG(APP_LOG_LEVEL_ERROR,"Canvas allocation failed");return;}
  layer_set_update_proc(s_canvas,paint);layer_add_child(root,s_canvas);refresh();
}
static void unload(Window *window) {
  if(s_canvas)layer_destroy(s_canvas);
  s_canvas=NULL;
}
void kit_shell_run(const LabDesign *design) {
  s_design=design;
#if LAB_SCENARIO == LAB_LIVE
  kit_init(refresh);
#endif
  if(s_design->load)s_design->load();
  s_window=window_create();
  if(!s_window){APP_LOG(APP_LOG_LEVEL_ERROR,"Window allocation failed");return;}
  window_set_background_color(s_window,GColorBlack);
  window_set_window_handlers(s_window,(WindowHandlers){.load=load,.unload=unload});
  window_stack_push(s_window,false);
  tick_timer_service_subscribe(MINUTE_UNIT,tick);
  accel_tap_service_subscribe(flick);
#if defined(PBL_TOUCH)
  touch_enable();
#endif
  battery_state_service_subscribe(battery);
  connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=connection});
  APP_LOG(APP_LOG_LEVEL_INFO,"Watchface kit 2.1.2 started (scenario %d, touch window %dms)",LAB_SCENARIO,KIT_TOUCH_WINDOW_MS);
  app_event_loop();
  tick_timer_service_unsubscribe();accel_tap_service_unsubscribe();
  battery_state_service_unsubscribe();connection_service_unsubscribe();
  if(s_detail_timer)app_timer_cancel(s_detail_timer);
#if defined(PBL_TOUCH)
  if(s_touch_subscribed)touch_service_unsubscribe();
#if KIT_TOUCH_WINDOW_MS > 0
  if(s_touch_timer)app_timer_cancel(s_touch_timer);
#endif
#endif
  window_destroy(s_window);
  if(s_design->unload)s_design->unload();
  lab_release_shared();
#if LAB_SCENARIO == LAB_LIVE
  kit_deinit();
#endif
}
#endif
