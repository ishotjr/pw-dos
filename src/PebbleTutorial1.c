#include <pebble.h>

// TODO: update with each release (major + zero-padded minor version)
#define VERSION_CODE 107  // v1.7

// broken into rows for easier editing
static const char WHATS_NEW_TEXT_01[] = "watch:~$ man pw-bash\n";
static const char WHATS_NEW_TEXT_02[] = "\n";
static const char WHATS_NEW_TEXT_03[] = "PW-BASH(1)\n";
static const char WHATS_NEW_TEXT_04[] = "\n";
static const char WHATS_NEW_TEXT_05[] = "NAME\n";
static const char WHATS_NEW_TEXT_06[] = "  pw-bash -- a silly\n";
static const char WHATS_NEW_TEXT_07[] = "  Pebble watchface \n";
static const char WHATS_NEW_TEXT_08[] = "DESCRIPTION\n";
static const char WHATS_NEW_TEXT_09[] = "  Initial release - \n";
static const char WHATS_NEW_TEXT_10[] = "  let us know your \n";
static const char WHATS_NEW_TEXT_11[] = "  feedback!\n";
static const char WHATS_NEW_TEXT_12[] = "\n";
static const char WHATS_NEW_TEXT_13[] = "  Shake to dismiss!";

// persistent storage keys
#define STORAGE_VERSION_CODE_KEY 1

// set text/cursor to P1 phosphor green on hardware that supports 64 colors, otherwise revert to white
// select correct splash
#ifdef PBL_COLOR
#define FOREGROUND_COLOR GColorFromHEX(0x56ff00)
#define TUX_SPLASH RESOURCE_ID_TUX
#else
#define FOREGROUND_COLOR GColorWhite
#define TUX_SPLASH RESOURCE_ID_TUX_BW
#endif

// (19 + \n)x13+\0
#define BUFFER_SIZE 261

// number of milliseconds cursor stays on/off, and total duration
#define BLINK_RATE_MS 533
#define BLINK_DURATION_MS 7500

static Window *s_main_window;
static TextLayer *s_time_layer;
// crude hack to mitigate sudden deprecation of InverterLayer in dp9
static TextLayer *s_cursor_layer;

static Window *s_whats_new_window;
static TextLayer *s_whats_new_layer;

static GFont s_time_font;

static int s_cursor_blink_count = 0;
static AppTimer *s_cursor_timer;

static int s_dir_frame_count = 0;
static AppTimer *s_dir_timer;

static AppTimer *s_splash_timer;

// persistent storage version (i.e. app version at last write)
static int s_storage_version_code = 0;

static BitmapLayer *s_tux_splash_layer;
static GBitmap *s_tux_splash_bitmap;

static void update_time(int frame) {
  // Get a tm structure
  time_t temp = time(NULL); 
  struct tm *tick_time = localtime(&temp);

  // Create a long-lived buffer
  static char buffer[BUFFER_SIZE];

  // TODO:
  //if(clock_is_24h_style() == true) {

  // TODO: this is horrendous/just very rough a prototype - refactor ASAP!

  // fake Date "typing"
  if (frame == 1)
      strftime(buffer, BUFFER_SIZE, "pebble OS tty1\nPW-BASH\n\nwatch@basalt:~$ d", tick_time); 
  else if (frame == 2)
      strftime(buffer, BUFFER_SIZE, "pebble OS tty1\nPW-BASH\n\nwatch@basalt:~$ da", tick_time);    
  else if (frame == 3)
      strftime(buffer, BUFFER_SIZE, "pebble OS tty1\nPW-BASH\n\nwatch@basalt:~$ dat", tick_time);
  else if (frame == 4)
      strftime(buffer, BUFFER_SIZE, "pebble OS tty1\nPW-BASH\n\nwatch@basalt:~$ date\n", tick_time);           
  else if (frame == -1)
      strftime(buffer, BUFFER_SIZE, "Bluetooth Disconnect\n", tick_time);
  else {
      // set "regular" screen again for remainder of the minute
      strftime(buffer, BUFFER_SIZE, "pebble OS tty1\nPW-BASH\n\nwatch@basalt:~$ date\n%a %b %d %T\nwatch@basalt:~$ ", tick_time);  
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

  // call again every 0.5s until last "typing" frame displayed, then reset to 0 and stop calling
  if (s_dir_frame_count <= 4) {
    s_dir_timer = app_timer_register(500, (AppTimerCallback) dir_timer_callback, NULL);
  } else {
    s_dir_frame_count = 0;
  }

}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed) {
  // set up DIR timer for :60 - frames*0.500
  s_dir_timer = app_timer_register(58 * 1000, (AppTimerCallback) dir_timer_callback, NULL);

  update_time(0);
}


static void cursor_timer_callback(void *data) {

  if (s_cursor_blink_count % 2) {
    // ~~InverterLayer~~ requires cast
    layer_set_hidden((Layer *)s_cursor_layer, false);
  } else {
    // ~~InverterLayer~~ requires cast
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

static void splash_timer_callback(void *data) {
  // hide splash after 3s
  layer_set_hidden((Layer *)s_tux_splash_layer, true);
}

static void tap_handler(AccelAxisType axis, int32_t direction) {
  // user shook or tapped Pebble (ignore axis/direction)

  if (window_is_loaded(s_whats_new_window)) {
    // during what's new

    // TODO: this seems like a "cheap" way of verifying - is it really OK?

    window_stack_pop(s_whats_new_window);

  } else {
    // during normal operation

    // kill existing timer in case of repeat shake during BLINK_DURATION_MS
    app_timer_cancel(s_cursor_timer);  
    // TODO: ^^^ add NULL setting/check?

    // kick off cursor blinking timer
    s_cursor_timer = app_timer_register(BLINK_RATE_MS, (AppTimerCallback) cursor_timer_callback, NULL);
  }

}


void bt_handler(bool connected) {
  // vibrate twice quickly on BT disconnect/connect
  if (connected) {
    vibes_double_pulse();

    // RE-Register with TickTimerService
    tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  
    // RE-Register with Tap Event Service
    accel_tap_service_subscribe(tap_handler);

    //APP_LOG(APP_LOG_LEVEL_INFO, "Phone is connected!");
  } else {
    // cancel any remaining timers
    app_timer_cancel(s_cursor_timer);
    app_timer_cancel(s_dir_timer);

    // disable TickTimerService until reconnection
    tick_timer_service_unsubscribe();

    // disable Tap Event Service until reconnection
    accel_tap_service_unsubscribe();

    vibes_double_pulse();

    // hide cursor in case lit
    layer_set_hidden((Layer *)s_cursor_layer, true);

    // visually alert user of disconnect status
    update_time(-1);

    //APP_LOG(APP_LOG_LEVEL_INFO, "Phone is not connected!");
  }
}


static void main_window_load(Window *window) {
  // Create time TextLayer
  s_time_layer = text_layer_create(GRect(0, 0, 144, 168));
  text_layer_set_background_color(s_time_layer, GColorBlack);

  text_layer_set_text_color(s_time_layer, FOREGROUND_COLOR);
  text_layer_set_overflow_mode(s_time_layer, GTextOverflowModeTrailingEllipsis);

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_CONSOLE_12));

  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);

  // Create cursor ~~InverterLayer~~

  // crude hack to mitigate sudden deprecation of InverterLayer in dp9
  // TODO: replace!
  s_cursor_layer = text_layer_create(GRect(7*16, 12*6+2-12, 7, 12));

  text_layer_set_background_color(s_cursor_layer, FOREGROUND_COLOR);

  // Add as child to time TextLayer
  layer_add_child(text_layer_get_layer(s_time_layer), text_layer_get_layer(s_cursor_layer));
  // hide initially
  layer_set_hidden((Layer *)s_cursor_layer, true);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));

  // load splash

  // Create GBitmap, then set to created BitmapLayer
  s_tux_splash_bitmap = gbitmap_create_with_resource(TUX_SPLASH);
  s_tux_splash_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_tux_splash_layer, s_tux_splash_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_tux_splash_layer));
}

static void main_window_unload(Window *window) {
  // cancel any remaining timers
  app_timer_cancel(s_cursor_timer);
  app_timer_cancel(s_dir_timer);

  // unsubscribe from Event Services
  tick_timer_service_unsubscribe();
  accel_tap_service_unsubscribe();
  bluetooth_connection_service_unsubscribe();

  // Destroy TextLayer
  text_layer_destroy(s_time_layer);

  // Destroy ~~InverterLayer~~
  text_layer_destroy(s_cursor_layer);

  // Unload GFont
  fonts_unload_custom_font(s_time_font);

  // // Destroy GBitmap
  gbitmap_destroy(s_tux_splash_bitmap);

  // // Destroy BitmapLayer
  bitmap_layer_destroy(s_tux_splash_layer);
}


static void whats_new_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create what's new TextLayer
  s_whats_new_layer = text_layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  text_layer_set_background_color(s_whats_new_layer, GColorBlack);
  text_layer_set_text_color(s_whats_new_layer, FOREGROUND_COLOR);
  text_layer_set_overflow_mode(s_whats_new_layer, GTextOverflowModeTrailingEllipsis);

  // (inherit from parent window (?))
  //// Create GFont
  //s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_14));

  // Apply to TextLayer
  text_layer_set_font(s_whats_new_layer, s_time_font);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_layer, text_layer_get_layer(s_whats_new_layer));

  // assemble WHATS_NEW_TEXT_*
  static char whats_new_buffer[BUFFER_SIZE];
  snprintf(whats_new_buffer, sizeof(whats_new_buffer), "%s%s%s%s%s%s%s%s%s%s%s%s%s", WHATS_NEW_TEXT_01, WHATS_NEW_TEXT_02,
    WHATS_NEW_TEXT_03, WHATS_NEW_TEXT_04, WHATS_NEW_TEXT_05, WHATS_NEW_TEXT_06, WHATS_NEW_TEXT_07,
    WHATS_NEW_TEXT_08, WHATS_NEW_TEXT_09, WHATS_NEW_TEXT_10, WHATS_NEW_TEXT_11, WHATS_NEW_TEXT_12,
    WHATS_NEW_TEXT_13);
  text_layer_set_text(s_whats_new_layer, whats_new_buffer);

  // 

}

static void whats_new_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_whats_new_layer);

  // start splash timer
  s_splash_timer = app_timer_register(3 * 1000, (AppTimerCallback) splash_timer_callback, NULL);
}


static void init() {

  // used to determine whether to display what's new
  bool is_new_version = false;

  // get initial storage version, or set to 0 if none
  s_storage_version_code = persist_exists(STORAGE_VERSION_CODE_KEY) ? persist_read_int(STORAGE_VERSION_CODE_KEY) : 0;

  // compare storage version to current version code
  if (s_storage_version_code < VERSION_CODE) {
    // storage is old

    // for now we use this to display "what's new" message
    is_new_version = true;

    // TODO: migration procedure? (or initialize if 0?)

    //REMINDER: update s_storage_version_code when complete!

    s_storage_version_code = VERSION_CODE;

  } else if (s_storage_version_code == VERSION_CODE) {
    // storage is current

    // nothing to do for now

    // TODO: load settings
  } else {
    // storage is ahead of app - should not be possible

    // for now, overwrite
    s_storage_version_code = VERSION_CODE;

    // TODO: revert to defaults?
  }



  // Create main Window element and assign to pointer
  s_main_window = window_create();

  // Set handlers to manage the elements inside the Window
  window_set_window_handlers(s_main_window, (WindowHandlers) {
    .load = main_window_load,
    .unload = main_window_unload
  });

  // Show the Window on the watch, with animated=true
  window_stack_push(s_main_window, true);

  // create even if unused as quick hack to allow window_is_loaded check in tap_handler
  // TODO: is this wasteful/lazy/bad?
  s_whats_new_window = window_create();

  // before we do anything else - check whether to display "what's new", and if so, push that too:
  if (is_new_version) {

    // create window/set handlers
    //s_whats_new_window = window_create();
    // ^^^ moved outside if for now

    window_set_window_handlers(s_whats_new_window, (WindowHandlers) {
      .load = whats_new_window_load,
      .unload = whats_new_window_unload
    });

    window_stack_push(s_whats_new_window, true);
  } else {
    // start splash timer
    s_splash_timer = app_timer_register(3 * 1000, (AppTimerCallback) splash_timer_callback, NULL);
  }

  // Make sure the time is displayed from the start
  update_time(0);

  // Register with TickTimerService
  tick_timer_service_subscribe(MINUTE_UNIT, tick_handler);
  //tick_timer_service_subscribe(SECOND_UNIT, tick_handler);

  // Register with Tap Event Service
  accel_tap_service_subscribe(tap_handler);

  // Register with Bluetooth Connection Service
  bluetooth_connection_service_subscribe(bt_handler);
}

static void deinit() {

  // persist storage version between launches
  persist_write_int(STORAGE_VERSION_CODE_KEY, s_storage_version_code);

  // Destroy Windows
  window_destroy(s_main_window);
  window_destroy(s_whats_new_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}
