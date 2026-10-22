#pragma once
#include "../health-stubs/pebble.h"
#define TRIG_MAX_RATIO 65536
typedef struct { int charge_percent; bool is_charging; } BatteryChargeState;
int32_t cos_lookup(int32_t angle);
BatteryChargeState battery_state_service_peek(void);
bool connection_service_peek_pebble_app_connection(void);
