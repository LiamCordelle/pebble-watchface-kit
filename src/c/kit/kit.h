#pragma once
#include "../lab.h"
#include "kit_core.h"

typedef struct {
  uint8_t version, time_mode, temp_unit, world;
  int32_t step_goal;
  uint8_t weather_enabled, demo_fallback;
  uint8_t large_labels, reserved;
} KitSettings;

// A design plugs in through LabDesign; no kit module depends on an individual design.
void kit_shell_run(const LabDesign *design);
void kit_init(void (*changed)(void));
void kit_deinit(void);
void kit_tick(void);
void kit_fill(LabData *data);
void kit_health_fill(LabData *data);
void kit_health_init(void);
void kit_health_deinit(void);
void kit_hr_history_update(time_t day,time_t now,bool invalidate);
void kit_hr_history_fill(LabData *data);
void kit_hr_history_clear(void);
const KitSettings *kit_settings_get(void);
void kit_settings_init(void);
void kit_settings_apply(LabData *data);
void kit_weather_init(void (*changed)(void));
void kit_weather_deinit(void);
void kit_weather_tick(void);
void kit_weather_fill(LabData *data);
void kit_weather_enabled(bool enabled);
void kit_weather_ready(void);

#ifndef LAB_HOST_RENDER
bool kit_tuple_int(DictionaryIterator *iter, uint32_t key, int32_t *value);
bool kit_settings_receive(DictionaryIterator *iter);
void kit_weather_receive(DictionaryIterator *iter);
void kit_weather_failed(DictionaryIterator *iter);
#endif
