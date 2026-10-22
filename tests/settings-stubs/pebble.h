#pragma once
#include "../health-stubs/pebble.h"
#include <stdio.h>
enum { MESSAGE_KEY_TIME_MODE, MESSAGE_KEY_TEMP_UNIT, MESSAGE_KEY_STEP_GOAL,
       MESSAGE_KEY_WORLD, MESSAGE_KEY_WEATHER_ENABLED, MESSAGE_KEY_DEMO_FALLBACK,
       MESSAGE_KEY_LARGE_LABELS };
enum { TUPLE_INT, TUPLE_UINT };
typedef union { int8_t int8; uint8_t uint8; int16_t int16; uint16_t uint16;
                int32_t int32; uint32_t uint32; } TupleValue;
typedef struct { int type; uint16_t length; TupleValue *value; } Tuple;
Tuple *dict_find(DictionaryIterator *, uint32_t);
int persist_get_size(uint32_t);
int persist_read_data(uint32_t, void *, size_t);
int persist_write_data(uint32_t, const void *, size_t);
bool clock_is_24h_style(void);
