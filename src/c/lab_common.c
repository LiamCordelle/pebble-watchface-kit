#include "lab.h"

static GFont s_pixel8;
static GFont s_pixel16;
static GDrawCommandImage *s_wx[LabWxUnknown];
static GDrawCommandImage *s_sun_rise;
static GDrawCommandImage *s_sun_set;

static const char *const DAY_NAMES[] = {
  "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
};
static const char *const DAY_ABBR[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
static const char *const MONTH_NAMES[] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};
static const char *const MONTH_ABBR[] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

int lab_clamp(int value, int lo, int hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

void lab_text(GContext *ctx, const char *text, GFont font, GRect box,
              GTextAlignment align, GColor color) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, font, box, GTextOverflowModeTrailingEllipsis, align, NULL);
}

GSize lab_text_size(const char *text, GFont font) {
  return graphics_text_layout_get_content_size(text, font, GRect(0, 0, 400, 200),
                                               GTextOverflowModeWordWrap,
                                               GTextAlignmentLeft);
}

void lab_fmt_time(char *buf, size_t n, int hour, int min, bool is_24h) {
  if (is_24h) {
    snprintf(buf, n, "%02d:%02d", hour, min);
    return;
  }
  int h12 = hour % 12;
  snprintf(buf, n, "%d:%02d", h12 == 0 ? 12 : h12, min);
}

void lab_fmt_minute_of_day(char *buf, size_t n, int minute_of_day, bool is_24h) {
  minute_of_day = lab_clamp(minute_of_day, 0, 1439);
  lab_fmt_time(buf, n, minute_of_day / 60, minute_of_day % 60, is_24h);
}

void lab_fmt_thousands(char *buf, size_t n, int32_t value) {
  bool negative = value < 0;
  uint32_t v = (uint32_t)(negative ? -value : value);
  if (v >= 1000000) {
    snprintf(buf, n, "%s%lu,%03lu,%03lu", negative ? "-" : "", (unsigned long)(v / 1000000),
             (unsigned long)((v / 1000) % 1000), (unsigned long)(v % 1000));
  } else if (v >= 1000) {
    snprintf(buf, n, "%s%lu,%03lu", negative ? "-" : "", (unsigned long)(v / 1000),
             (unsigned long)(v % 1000));
  } else {
    snprintf(buf, n, "%s%lu", negative ? "-" : "", (unsigned long)v);
  }
}

void lab_fmt_k(char *buf, size_t n, int32_t value) {
  if (value < 0) {
    snprintf(buf, n, "--");
  } else if (value >= 10000) {
    snprintf(buf, n, "%ldk", (long)(value / 1000));
  } else if (value >= 1000) {
    snprintf(buf, n, "%ld.%ldk", (long)(value / 1000), (long)((value % 1000) / 100));
  } else {
    snprintf(buf, n, "%ld", (long)value);
  }
}

void lab_upper(char *s) {
  for (; s && *s; s++) {
    if (*s >= 'a' && *s <= 'z') {
      *s = (char)(*s - 32);
    }
  }
}

const char *lab_day_name(int wday) {
  return DAY_NAMES[lab_clamp(wday, 0, 6)];
}

const char *lab_day_abbr(int wday) {
  return DAY_ABBR[lab_clamp(wday, 0, 6)];
}

const char *lab_month_name(int mon) {
  return MONTH_NAMES[lab_clamp(mon, 0, 11)];
}

const char *lab_month_abbr(int mon) {
  return MONTH_ABBR[lab_clamp(mon, 0, 11)];
}

LabWxKind lab_weather_kind(int code) {
  if (code < 0) return LabWxUnknown;
  if (code == 0) return LabWxClear;
  if (code <= 2) return LabWxPartly;
  if (code == 3) return LabWxCloudy;
  if (code <= 48) return LabWxFog;
  if (code <= 67) return LabWxRain;
  if (code <= 77) return LabWxSnow;
  if (code <= 82) return LabWxRain;
  if (code <= 86) return LabWxSnow;
  if (code <= 99) return LabWxStorm;
  return LabWxUnknown;
}

const char *lab_weather_text(int code) {
  if (code < 0) return "Unknown";
  if (code == 0) return "Clear";
  if (code == 1) return "Mostly clear";
  if (code == 2) return "Partly cloudy";
  if (code == 3) return "Overcast";
  if (code <= 48) return "Fog";
  if (code <= 57) return "Drizzle";
  if (code <= 67) return "Rain";
  if (code <= 77) return "Snow";
  if (code <= 82) return "Showers";
  if (code <= 86) return "Snow showers";
  if (code <= 99) return "Thunderstorm";
  return "Unknown";
}

GColor lab_battery_color(int pct) {
  if (pct < 10) {
    return GColorFromHEX(0xff0000);
  }
  if (pct < 40) {
    return GColorFromHEX(0xffaa00);
  }
  return GColorFromHEX(0x00aa55);
}

GPoint lab_polar(GPoint center, int radius, int32_t angle) {
  return GPoint(center.x + (sin_lookup(angle) * radius) / TRIG_MAX_RATIO,
                center.y - (cos_lookup(angle) * radius) / TRIG_MAX_RATIO);
}

static GDrawCommandImage *weather_image(LabWxKind kind) {
  static const uint32_t ids[LabWxUnknown] = {
    RESOURCE_ID_WX_SUN, RESOURCE_ID_WX_PARTLY, RESOURCE_ID_WX_CLOUDY, RESOURCE_ID_WX_FOG,
    RESOURCE_ID_WX_RAIN, RESOURCE_ID_WX_SNOW, RESOURCE_ID_WX_STORM
  };
  if (kind == LabWxUnknown) {
    kind = LabWxCloudy;
  }
  if (!s_wx[kind]) {
    s_wx[kind] = gdraw_command_image_create_with_resource(ids[kind]);
  }
  return s_wx[kind];
}

void lab_draw_weather_pdc(GContext *ctx, int weather_code, GPoint origin) {
  GDrawCommandImage *img = weather_image(lab_weather_kind(weather_code));
  if (img) {
    gdraw_command_image_draw(ctx, img, origin);
  }
}

void lab_draw_sun_pdc(GContext *ctx, bool sunset, GPoint origin) {
  GDrawCommandImage **slot = sunset ? &s_sun_set : &s_sun_rise;
  if (!*slot) {
    *slot = gdraw_command_image_create_with_resource(sunset ? RESOURCE_ID_SUN_SET
                                                            : RESOURCE_ID_SUN_RISE);
  }
  if (*slot) {
    gdraw_command_image_draw(ctx, *slot, origin);
  }
}

GFont lab_font_pixel8(void) {
  if (!s_pixel8) {
    s_pixel8 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_8));
  }
  return s_pixel8;
}

GFont lab_font_pixel16(void) {
  if (!s_pixel16) {
    s_pixel16 = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PIXEL_16));
  }
  return s_pixel16;
}

void lab_release_shared(void) {
  if (s_pixel8) {
    fonts_unload_custom_font(s_pixel8);
    s_pixel8 = NULL;
  }
  if (s_pixel16) {
    fonts_unload_custom_font(s_pixel16);
    s_pixel16 = NULL;
  }
  for (int i = 0; i < LabWxUnknown; i++) {
    if (s_wx[i]) {
      gdraw_command_image_destroy(s_wx[i]);
      s_wx[i] = NULL;
    }
  }
  if (s_sun_rise) {
    gdraw_command_image_destroy(s_sun_rise);
    s_sun_rise = NULL;
  }
  if (s_sun_set) {
    gdraw_command_image_destroy(s_sun_set);
    s_sun_set = NULL;
  }
}
