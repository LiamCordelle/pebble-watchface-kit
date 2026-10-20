#pragma once
// Minimal SDK surface for compiling the real kit_health.c on the host.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
typedef struct GContext GContext;
typedef struct DictionaryIterator DictionaryIterator;
typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *);
AppTimer *app_timer_register(uint32_t,AppTimerCallback,void *);
bool app_timer_cancel(AppTimer *);
typedef struct {
  uint8_t steps,orientation;
  uint16_t vmc;
  bool is_invalid;
  uint8_t light,padding,heart_rate_bpm,reserved;
} HealthMinuteData;
uint32_t health_service_get_minute_history(HealthMinuteData *,uint32_t,time_t *,time_t *);
typedef void *GFont;
typedef uint8_t GColor;
typedef int GTextAlignment;
typedef struct { int16_t x,y; } GPoint;
typedef struct { int16_t w,h; } GSize;
typedef struct { GPoint origin; GSize size; } GRect;
typedef enum {
  HealthMetricStepCount, HealthMetricActiveSeconds, HealthMetricWalkedDistanceMeters,
  HealthMetricSleepSeconds, HealthMetricActiveKCalories, HealthMetricHeartRateBPM
} HealthMetric;
typedef int32_t HealthValue;
typedef enum { HealthAggregationSum, HealthAggregationAvg, HealthAggregationMin,
               HealthAggregationMax } HealthAggregation;
typedef enum { HealthServiceTimeScopeOnce, HealthServiceTimeScopeDailyWeekdayOrWeekend }
  HealthServiceTimeScope;
typedef enum {
  HealthServiceAccessibilityMaskAvailable=1, HealthServiceAccessibilityMaskNoPermission=2,
  HealthServiceAccessibilityMaskNotSupported=4, HealthServiceAccessibilityMaskNotAvailable=8
} HealthServiceAccessibilityMask;
typedef enum { HealthEventSignificantUpdate, HealthEventMovementUpdate,
               HealthEventHeartRateUpdate, HealthEventSleepUpdate } HealthEventType;
typedef void (*HealthEventHandler)(HealthEventType,void *);
HealthServiceAccessibilityMask health_service_metric_accessible(HealthMetric,time_t,time_t);
HealthServiceAccessibilityMask health_service_metric_aggregate_averaged_accessible(
    HealthMetric,time_t,time_t,HealthAggregation,HealthServiceTimeScope);
HealthValue health_service_sum(HealthMetric,time_t,time_t);
HealthValue health_service_aggregate_averaged(
    HealthMetric,time_t,time_t,HealthAggregation,HealthServiceTimeScope);
HealthValue health_service_peek_current_value(HealthMetric);
bool health_service_events_subscribe(HealthEventHandler,void *);
void health_service_events_unsubscribe(void);
#define APP_LOG(...) ((void)0)
