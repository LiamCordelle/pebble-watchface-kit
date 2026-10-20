#include "kit.h"
#ifndef LAB_HOST_RENDER
static void (*s_changed)(void);

static void inbox(DictionaryIterator *iter, void *context) {
  bool changed = kit_settings_receive(iter);
  if (changed) kit_weather_enabled(kit_settings_get()->weather_enabled);
  int32_t ready;
  if (kit_tuple_int(iter, MESSAGE_KEY_READY, &ready) && ready == KIT_VERSION) kit_weather_ready();
  kit_weather_receive(iter);
  if (changed && s_changed) s_changed();
}
static void dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "Kit inbox dropped %d; watchdog will recover", reason);
}
static void failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_INFO, "Kit outbox retry: %d", reason);
  kit_weather_failed(iter);
}
void kit_init(void (*changed)(void)) {
  s_changed = changed;
  kit_settings_init();
  kit_health_init();
  kit_weather_init(changed);
  app_message_register_inbox_received(inbox);
  app_message_register_inbox_dropped(dropped);
  app_message_register_outbox_failed(failed);
  AppMessageResult result = app_message_open(512, 64);
  if (result != APP_MSG_OK) APP_LOG(APP_LOG_LEVEL_ERROR, "Kit AppMessage open failed %d", result);
}
void kit_tick(void) { kit_weather_tick(); }
void kit_fill(LabData *d) {
  kit_health_fill(d);
  kit_settings_apply(d);
  kit_weather_fill(d);
}
void kit_deinit(void) {
  kit_weather_deinit();
  kit_health_deinit();
  app_message_deregister_callbacks();
  s_changed = NULL;
}
#endif
