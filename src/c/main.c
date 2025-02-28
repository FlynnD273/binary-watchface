#include <pebble.h>

static Window *s_main_window;

static Layer *s_minute_layer;
static Layer *s_hour_layer;
#ifndef PBL_PLATFORM_CHALK
static Layer *s_battery_layer;
#endif

static int minute = 0;
static int hour = 0;
static uint8_t batt_percent = 0;

/**
 * Redraw the time UI elements
 */
static void handle_minute_tick(struct tm *tick_time, TimeUnits units_changed) {
  if (units_changed & MINUTE_UNIT) {
    minute = tick_time->tm_min;
    layer_mark_dirty(s_minute_layer);
  }
  if (units_changed & HOUR_UNIT) {
    hour = tick_time->tm_hour;
    // Convert 24 to 12 hour time
    hour %= 12;
    if (hour == 0) {
      hour = 12;
    }
    layer_mark_dirty(s_hour_layer);
  }
}

static void draw_binary(Layer *layer, GContext *ctx, int digits, int number) {
  GRect bounds = layer_get_frame(layer);
  int radius = 7;
  int y = bounds.size.h / 2;
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_context_set_stroke_color(ctx, GColorBlack);
  graphics_context_set_stroke_width(ctx, 2);
  for (int i = 0; i < digits; i++) {
    int x = bounds.origin.x + bounds.size.w * (digits - i) / (digits + 1);
    if ((number >> i) & 1) {
      graphics_fill_circle(ctx, GPoint(x, y), radius);
    } else {
      graphics_draw_circle(ctx, GPoint(x, y), radius);
    }
  }
}

static void minute_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_frame(layer);
  draw_binary(layer, ctx, 6, minute);
}

static void hour_update_proc(Layer *layer, GContext *ctx) {
  draw_binary(layer, ctx, 4, hour);
}

#ifndef PBL_PLATFORM_CHALK
/**
 * Called when the battery level changes.
 */
static void handle_battery(BatteryChargeState charge_state) {
  batt_percent = charge_state.charge_percent;
  layer_mark_dirty(s_battery_layer);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
  GRect bounds = layer_get_bounds(layer);
  GRect batt_bar =
      GRect(0, 0, bounds.size.w * batt_percent / 100, bounds.size.h);
  graphics_context_set_fill_color(ctx, GColorBlack);
  // Main bar
  graphics_fill_rect(ctx, batt_bar, 0, 0);

  // Little pips to make it easier to see
  for (uint8_t i = 0; i <= 100; i += 20) {
    if (i < batt_percent) {
      graphics_context_set_stroke_color(ctx, GColorWhite);
    } else {
      graphics_context_set_stroke_color(ctx, GColorBlack);
    }
    uint8_t x = (bounds.size.w - 1) * i / 100;
    graphics_draw_line(ctx, GPoint(x, 0), GPoint(x, bounds.size.h));
  }
}
#endif

static void main_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  s_hour_layer = layer_create(GRect(0, 0, bounds.size.w, bounds.size.h / 2));
  layer_set_update_proc(s_hour_layer, hour_update_proc);

  s_minute_layer = layer_create(
      GRect(0, bounds.size.h / 4, bounds.size.w, bounds.size.h / 2));
  layer_set_update_proc(s_minute_layer, minute_update_proc);

  // Ensures time is displayed immediately (will break if NULL tick event
  // accessed). (This is why it's a good idea to have a separate routine to do
  // the update itself.)
  time_t now = time(NULL);
  struct tm *current_time = localtime(&now);
  handle_minute_tick(current_time, MINUTE_UNIT | HOUR_UNIT);

  tick_timer_service_subscribe(MINUTE_UNIT | HOUR_UNIT, handle_minute_tick);

#ifndef PBL_PLATFORM_CHALK
  GRect batt_frame = GRect(0, bounds.size.h - 5, bounds.size.w, 5);
  s_battery_layer = layer_create(batt_frame);
  layer_set_update_proc(s_battery_layer, battery_update_proc);
  handle_battery(battery_state_service_peek());
  battery_state_service_subscribe(handle_battery);
  layer_add_child(window_layer, s_battery_layer);
#endif

  layer_add_child(window_layer, s_minute_layer);
  layer_add_child(window_layer, s_hour_layer);
}

static void main_window_unload(Window *window) {
  tick_timer_service_unsubscribe();
  battery_state_service_unsubscribe();
  layer_destroy(s_minute_layer);
  layer_destroy(s_hour_layer);
#ifndef PBL_PLATFORM_CHALK
  layer_destroy(s_battery_layer);
#endif
}

static void init() {
  s_main_window = window_create();
  window_set_background_color(s_main_window, GColorWhite);
  window_set_window_handlers(s_main_window, (WindowHandlers){
                                                .load = main_window_load,
                                                .unload = main_window_unload,
                                            });
  window_stack_push(s_main_window, true);
}

static void deinit() { window_destroy(s_main_window); }

int main(void) {
  init();
  app_event_loop();
  deinit();
}
