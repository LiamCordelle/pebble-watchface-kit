#pragma once
#include <pebble.h>

// ---------------------------------------------------------------------------
// Data scenarios, chosen at build time:  LAB_SCENARIO=<n> pebble build
//   0 demo   - fixed Saturday-morning data, used for gallery screenshots
//   1 stress - widest/longest values, to catch truncation and overflow
//   2 empty  - no weather, no heart rate, zero steps, low battery, BT off
//   3 live   - real watch sensors (weather stays on demo values: no phone JS)
// ---------------------------------------------------------------------------
#ifndef LAB_SCENARIO
#define LAB_SCENARIO 0
#endif

#define LAB_DEMO 0
#define LAB_STRESS 1
#define LAB_EMPTY 2
#define LAB_LIVE 3

typedef struct {
  // Time ---------------------------------------------------------------------
  struct tm now;              // local wall-clock time (tm_wday 0 = Sunday)
  time_t epoch;               // UTC seconds for `now`
  bool is_24h;                // user's clock preference
  int minute_of_day;          // 0..1439

  // Second time zone (Signal Deck default: NZ) ------------------------------
  char world_label[6];        // "NZ"
  int world_hour;             // 0..23
  int world_min;              // 0..59
  int world_day_delta;        // -1, 0 or +1 relative to the local date

  // Health -------------------------------------------------------------------
  bool steps_ok;
  int32_t steps;
  int32_t step_goal;
  int32_t steps_typical;      // usual steps by this time of day, 0 = unknown
  uint16_t steps_by_hour[24]; // today's steps per clock hour; future hours = 0
  bool hr_ok;
  int heart_rate;             // bpm (valid when hr_ok)
  int hr_min;                 // today's lowest recorded minute BPM, 0 = unknown
  int hr_max;                 // today's highest recorded minute BPM, 0 = unknown
  int active_minutes;
  int32_t distance_m;
  int active_kcal;
  int sleep_minutes;          // last night

  // Device -------------------------------------------------------------------
  int battery_pct;            // true 0..100 (Signal Deck caps the label at 99%)
  bool charging;
  bool bt_connected;

  // Weather (Celsius) --------------------------------------------------------
  bool weather_ok;
  int temp;
  int temp_hi;
  int temp_lo;
  int weather_code;           // WMO: 0 clear, 1-3 cloud, 45/48 fog, 51-67 rain,
                              // 71-77 snow, 80-82 showers, 85/86 snow showers,
                              // 95-99 thunder
  int rain_pct;               // precipitation probability, current hour
  int uv_x10;                 // UV index x10 (30 = 3.0)
  int humidity_pct;
  int wind_kph;
  int wind_from_deg;          // direction the wind blows FROM, 0 = north
  int sunrise_min;            // minutes after local midnight
  int sunset_min;
  int8_t temp_by_hour[24];    // today's hourly temperature
  uint8_t rain_by_hour[24];   // today's hourly precipitation probability

  // Sky ----------------------------------------------------------------------
  int moon_phase_e3;          // 0 new, 250 first quarter, 500 full, 750 last
  int moon_illum_pct;         // 0..100

  // History ------------------------------------------------------------------
  uint16_t steps_by_day[28];  // steps per day: [0] today so far, [1] yesterday, ...
  uint8_t hr_by_hour[24];     // today's average bpm per clock hour, 0 = no reading

  // Kit 1.0 additions. Canonical weather is Celsius; convert at drawing time.
  bool temp_fahrenheit;
  bool large_labels;          // opt-in: preserve the approved original by default
  int tomorrow_sunrise_min;
  int tomorrow_sunset_min;
  int next_sun_min;
  bool next_sun_is_set;
  int next_sun_day_delta;
  int uv_max_x10;
  int8_t rolling_temp[24];
  uint8_t rolling_rain[24];
  uint8_t rolling_code[24];
  uint8_t rolling_hour[24], rolling_day[24]; // local labels, including DST transitions
  uint8_t rolling_count;
  bool hr_range_partial;      // kit 1.3: loading, missing records or read failure
} LabData;

// One watchface design. Everything except this struct stays `static` in the
// design's own file, so all designs can later be linked into one app.
typedef struct {
  const char *name;
  void (*load)(void);         // load fonts/images; may be NULL
  void (*unload)(void);       // free them; may be NULL
  void (*draw)(GContext *ctx, GRect bounds, const LabData *d);
  // Optional "tap for detail" view. The shell shows it when the wearer taps
  // the screen (or flicks the wrist in a single-design build) and returns to
  // `draw` after about 8 seconds or another tap. NULL = no detail view.
  void (*draw_detail)(GContext *ctx, GRect bounds, const LabData *d);
} LabDesign;

#define LAB_HAS_DETAIL 1

// Data (lab_data.c) -----------------------------------------------------------
void lab_data_refresh(LabData *d);

// Helpers (lab_common.c) ------------------------------------------------------
int lab_clamp(int value, int lo, int hi);

// Text drawn with trailing-ellipsis overflow; measure before drawing if a
// value could get wide.
void lab_text(GContext *ctx, const char *text, GFont font, GRect box,
              GTextAlignment align, GColor color);
GSize lab_text_size(const char *text, GFont font);

// "10:09" in 24h, "10:09" / "9:05" in 12h (no leading zero).
void lab_fmt_time(char *buf, size_t n, int hour, int min, bool is_24h);
void lab_fmt_minute_of_day(char *buf, size_t n, int minute_of_day, bool is_24h);
void lab_fmt_thousands(char *buf, size_t n, int32_t value);   // 6,842
void lab_fmt_k(char *buf, size_t n, int32_t value);           // 6.8k, 18k
void lab_upper(char *s);

const char *lab_day_name(int wday);      // "Saturday"
const char *lab_day_abbr(int wday);      // "Sat"
const char *lab_month_name(int mon);     // "October" (tm_mon 0..11)
const char *lab_month_abbr(int mon);     // "Oct"

typedef enum {
  LabWxClear,
  LabWxPartly,
  LabWxCloudy,
  LabWxFog,
  LabWxRain,
  LabWxSnow,
  LabWxStorm,
  LabWxUnknown
} LabWxKind;

LabWxKind lab_weather_kind(int weather_code);
const char *lab_weather_text(int weather_code);   // "Partly cloudy"

// Signal Deck battery convention: green, amber below 40%, red below 10%.
GColor lab_battery_color(int pct);

// Point on a circle; angle 0 = 12 o'clock, clockwise, TRIG_MAX_ANGLE = 360°.
GPoint lab_polar(GPoint center, int radius, int32_t angle);

// Signal Deck's colour PDC icons: weather 30x30, sunrise/sunset 24x20.
// Drawing a PDC leaves the context's stroke width and colours changed, so set
// them again afterwards. The snow flakes are white and the storm bolt is pale
// yellow, so both vanish on light grounds.
void lab_draw_weather_pdc(GContext *ctx, int weather_code, GPoint origin);
void lab_draw_sun_pdc(GContext *ctx, bool sunset, GPoint origin);

// Shared Silkscreen pixel fonts (crisp at 8/16 px). Loaded on first use and
// freed by the shell when the design changes - never unload them yourself.
GFont lab_font_pixel8(void);    // Silkscreen Bold 8
GFont lab_font_pixel16(void);   // Silkscreen Regular 16
void lab_release_shared(void);
