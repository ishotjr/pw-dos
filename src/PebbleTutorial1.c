#include <pebble.h>

// 18x12+\0
#define BUFFER_SIZE 217

// number of milliseconds cursor stays on/off, and total duration
#define BLINK_RATE_MS 533
#define BLINK_DURATION_MS 7500

static Window *s_main_window;
static TextLayer *s_time_layer;
static InverterLayer *s_cursor_layer;

static GFont s_time_font;

static int s_cursor_blink_count = 0;
static AppTimer *s_cursor_timer;

static int s_dir_frame_count = 0;
static AppTimer *s_dir_timer;

// static BitmapLayer *s_starman_layer;
// static GBitmap *s_starman_bitmap;

static void update_time(int frame) {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[BUFFER_SIZE];

  // TODO:
  //if(clock_is_24h_style() == true) {

  // TODO: this is horrendous/just very rough a prototype - refactor ASAP!

  // fake DIR "typing"
  if (frame == 1)
      strftime(buffer, BUFFER_SIZE, "PW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>D", tick_time);    
  else if (frame == 2)
      strftime(buffer, BUFFER_SIZE, "PW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>DI", tick_time);    
  else if (frame == 3)
      strftime(buffer, BUFFER_SIZE, "PW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>DIR", tick_time);    
  // fake "scroll" after DIR
  else if (frame == 4)
      strftime(buffer, BUFFER_SIZE, "AUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>DIR\nPW-DOS %d.%m\nCopyright (c) %Y\n\n", tick_time);    
  else if (frame == 5)
      strftime(buffer, BUFFER_SIZE, "\n 3 files %j bytes\n %U bytes free\n\nC:\\>DIR\nPW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M", tick_time);    
  else if (frame == 6)
      strftime(buffer, BUFFER_SIZE, "C:\\>DIR\n\n\nPW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free", tick_time);    
  else {
      // set "regular" screen again for remainder of the minute
      strftime(buffer, BUFFER_SIZE, "PW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>", tick_time);    
      //layer_set_hidden((Layer *)s_cursor_layer, false);
  }

  // Display this time on the TextLayer
  text_layer_set_text(s_time_layer, buffer);
}


static void dir_timer_callback(void *data) {

  // kill cursor timer throughout DIR animation (user could shake during) and force hidden 
  // (to prevent floating cursor)
  app_timer_cancel(s_cursor_timer);
  layer_set_hidden((Layer *)s_cursor_layer, true);

  // increment frame count and update display
  update_time(++s_dir_frame_count);

  // call again every 0.5s until 6th frame displayed, then reset to 0 and stop calling
  if (s_dir_frame_count <= 6) {
    s_dir_timer = app_timer_register(500, (AppTimerCallback) dir_timer_callback, NULL);
  } else {
    s_dir_frame_count = 0;
  }

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // set up DIR timer for :57
  s_dir_timer = app_timer_register(57 * 1000, (AppTimerCallback) dir_timer_callback, NULL);

  update_time(0);
}


static void cursor_timer_callback(void *data) {

  if (s_cursor_blink_count % 2) {
    // InverterLayer requires cast
    layer_set_hidden((Layer *)s_cursor_layer, false);
  } else {
    // InverterLayer requires cast
    layer_set_hidden((Layer *)s_cursor_layer, true);
  }

  // increment count and call again until duration elapses, then reset to 0 and stop calling
  if (s_cursor_blink_count < (BLINK_DURATION_MS / BLINK_RATE_MS)) {
    s_cursor_blink_count++;
    s_cursor_timer = app_timer_register(BLINK_RATE_MS, (AppTimerCallback) cursor_timer_callback, NULL);
  } else {
    s_cursor_blink_count = 0;
  }

}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  // user shook or tapped Pebble (ignore axis/direction)

  // kill existing timer in case of repeat shake during BLINK_DURATION_MS
  app_timer_cancel(s_cursor_timer);  
  // TODO: ^^^ add NULL setting/check?

  // kick off cursor blinking timer
  s_cursor_timer = app_timer_register(BLINK_RATE_MS, (AppTimerCallback) cursor_timer_callback, NULL);

}


static void main_window_load(Window *window) {
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(0, 0, 144, 168));
  text_layer_set_background_color(s_time_layer, GColorBlack);
  text_layer_set_text_color(s_time_layer, GColorClear);
  text_layer_set_overflow_mode(s_time_layer, GTextOverflowModeTrailingEllipsis);

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_14));

  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);

  // Create cursor InverterLayer
  s_cursor_layer = inverter_layer_create(GRect(32, 141, 7, 1));
  // Add as child to time TextLayer
  layer_add_child(text_layer_get_layer(s_time_layer), inverter_layer_get_layer(s_cursor_layer));
  // hide initially
  layer_set_hidden((Layer *)s_cursor_layer, true);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));

  // // Create GBitmap, then set to created BitmapLayer
  // s_starman_bitmap = gbitmap_create_with_resource(RESOURCE_ID_STARMAN);
  // s_starman_layer = bitmap_layer_create(GRect(104, 128, 40, 40));
  // bitmap_layer_set_bitmap(s_starman_layer, s_starman_bitmap);
  // layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_starman_layer));
}

static void main_window_unload(Window *window) {
  // cancel any remaining timers
  app_timer_cancel(s_cursor_timer);
  app_timer_cancel(s_dir_timer);

  // Destroy TextLayer
  text_layer_destroy(s_time_layer);

  // Destroy InverterLayer
  inverter_layer_destroy(s_cursor_layer);

  // Unload GFont
  fonts_unload_custom_font(s_time_font);

  // // Destroy GBitmap
  // gbitmap_destroy(s_starman_bitmap);

  // // Destroy BitmapLayer
  // bitmap_layer_destroy(s_starman_layer);
}

static void init() {
  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // Make sure the time is displayed from the start
  update_time(0);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  //tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Register with Tap Event Service
  accel_tap_service_subscribe(tap_handler);
}

static void deinit() {
    // Destroy Window
    window_destroy(s_main_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}