#include <pebble.h>

// TODO: update with each release (major + zero-padded minor version)
#define VERSION_CODE 110  // v1.10

// broken into rows for easier editing
static const char WHATS_NEW_TEXT_01[] = "+----------------+\n";
static const char WHATS_NEW_TEXT_02[] = "| ? WHAT'S NEW ? |\n";
static const char WHATS_NEW_TEXT_03[] = "+----------------+\n";
static const char WHATS_NEW_TEXT_04[] = "| Updated for    |\n";
static const char WHATS_NEW_TEXT_05[] = "| SDK 4.0 inclu- |\n";
static const char WHATS_NEW_TEXT_06[] = "| ding Pebble 2  |\n";
static const char WHATS_NEW_TEXT_07[] = "| (Diorite)      |\n";
static const char WHATS_NEW_TEXT_08[] = "| support! :D    |\n";
static const char WHATS_NEW_TEXT_09[] = "+----------------+\n";
static const char WHATS_NEW_TEXT_10[] = "| Please shake   |\n";
static const char WHATS_NEW_TEXT_11[] = "| to dismiss...  |\n";
static const char WHATS_NEW_TEXT_12[] = "+----------------+\n";

// persistent storage keys
#define STORAGE_VERSION_CODE_KEY 1
#define STORAGE_FOREGROUND_COLOR_KEY 2


// (18 + \n)x12+\0
#define BUFFER_SIZE 229

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

static BitmapLayer *s_pwbios_splash_layer;
static GBitmap *s_pwbios_splash_bitmap;


// actual *types* differ between platforms!
#ifdef PBL_COLOR
  static GColor8 s_foreground_color;
#else
  static GColor s_foreground_color;
#endif


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
  else if (frame == -1)
      strftime(buffer, BUFFER_SIZE, "\nNot ready reading drive B at %H:%M\n\nAbort,Retry,Fail?", tick_time);    
  else if (frame == -2)
      strftime(buffer, BUFFER_SIZE, "PW-DOS %d.%m\nCopyright (c) %Y\n\nAUTOEXEC BAT %H:%M\nCOMMAND  COM %H:%M\nCONFIG   SYS %H:%M\n 3 files %j bytes\n %U bytes free\n\nC:\\>TOGGLE.BAT\n\nC:\\>", tick_time);    
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

  // quick hack to clear TOGGLE.BAT
  if (s_cursor_blink_count % 2) {
    update_time(0);
  }

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
  layer_set_hidden((Layer *)s_pwbios_splash_layer, true);
}


static void tap_handler(AccelAxisType axis, int32_t direction) {
  // user shook or tapped Pebble (ignore axis/direction)

  if (window_is_loaded(s_whats_new_window)) {
    // during what's new

    // TODO: this seems like a "cheap" way of verifying - is it really OK?

    window_stack_pop(s_whats_new_window);

  } else if (s_cursor_blink_count > 0) {
    // we're in the middle of blinking - change color in response to double-shake!

#ifdef PBL_COLOR
    if (gcolor_equal(s_foreground_color, GColorBrightGreen)) {
      s_foreground_color = GColorChromeYellow;      
    } else {
      s_foreground_color = GColorBrightGreen;      
    }

    // TODO: more colors later!

    // (notes)

//P1 phosphor green
//#define FOREGROUND_COLOR GColorFromHEX(0x56ff00)

// Bright Green - almost the same color
//#define FOREGROUND_COLOR GColorBrightGreen

// Green
//#define FOREGROUND_COLOR GColorGreen

// P3 phosphor amber
//#define FOREGROUND_COLOR GColorFromHEX(0xffb700)

// Chrome Yellow
//#define FOREGROUND_COLOR GColorChromeYellow

// Red
//#define FOREGROUND_COLOR GColorRed


    // update layers with new color
    text_layer_set_text_color(s_time_layer, s_foreground_color);  
    text_layer_set_background_color(s_cursor_layer, s_foreground_color);
    // but NOT s_whats_new_layer!

    s_cursor_blink_count = 0; // abort blink if color set???  
    // TODO: not sure if this even makes sense / how it jives w/ timer event?
    app_timer_cancel(s_cursor_timer);  

    // ensure no hanging cursor
    layer_set_hidden((Layer *)s_cursor_layer, true);

    // silly fake batch file
    update_time(-2);

#endif
    // do nothing for aplite
  }
  else {
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

  text_layer_set_text_color(s_time_layer, s_foreground_color);
  text_layer_set_overflow_mode(s_time_layer, GTextOverflowModeTrailingEllipsis);

  // Create GFont
  s_time_font = fonts_load_custom_font(resource_get_handle(RESOURCE_ID_FONT_PERFECT_DOS_14));

  // Apply to TextLayer
  text_layer_set_font(s_time_layer, s_time_font);

  // Create cursor ~~InverterLayer~~

  // crude hack to mitigate sudden deprecation of InverterLayer in dp9
  // TODO: replace!
  s_cursor_layer = text_layer_create(GRect(32, 141, 7, 1));

  APP_LOG(APP_LOG_LEVEL_DEBUG, "text_layer_set_background_color() %d", s_foreground_color.argb);
  text_layer_set_background_color(s_cursor_layer, s_foreground_color);

  // Add as child to time TextLayer
  layer_add_child(text_layer_get_layer(s_time_layer), text_layer_get_layer(s_cursor_layer));
  // hide initially
  layer_set_hidden((Layer *)s_cursor_layer, true);

  // Add it as a child layer to the Window's root layer
  layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_time_layer));


  // load splash

  // Create GBitmap, then set to created BitmapLayer
  s_pwbios_splash_bitmap = gbitmap_create_with_resource(RESOURCE_ID_PWBIOS_SPLASH);
  s_pwbios_splash_layer = bitmap_layer_create(GRect(0, 0, 144, 168));
  bitmap_layer_set_bitmap(s_pwbios_splash_layer, s_pwbios_splash_bitmap);
  layer_add_child(window_get_root_layer(window), bitmap_layer_get_layer(s_pwbios_splash_layer));

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
  // gbitmap_destroy(s_starman_bitmap);

  // // Destroy BitmapLayer
  // bitmap_layer_destroy(s_starman_layer);
}


static void whats_new_window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect window_bounds = layer_get_bounds(window_layer);

  // Create what's new TextLayer
  s_whats_new_layer = text_layer_create(GRect(0, 0, window_bounds.size.w, window_bounds.size.h));
  text_layer_set_background_color(s_whats_new_layer, GColorBlack);

  // v1.9 only! demo new amber color via what's new!
#ifdef PBL_COLOR
  text_layer_set_text_color(s_whats_new_layer, GColorChromeYellow);
#else
  text_layer_set_text_color(s_whats_new_layer, s_foreground_color);  
#endif
  // TODO: revert after v1.9!!
  //text_layer_set_text_color(s_whats_new_layer, s_foreground_color);
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
  snprintf(whats_new_buffer, sizeof(whats_new_buffer), "%s%s%s%s%s%s%s%s%s%s%s%s", WHATS_NEW_TEXT_01, WHATS_NEW_TEXT_02,
    WHATS_NEW_TEXT_03, WHATS_NEW_TEXT_04, WHATS_NEW_TEXT_05, WHATS_NEW_TEXT_06, WHATS_NEW_TEXT_07,
    WHATS_NEW_TEXT_08, WHATS_NEW_TEXT_09, WHATS_NEW_TEXT_10, WHATS_NEW_TEXT_11, WHATS_NEW_TEXT_12);
  text_layer_set_text(s_whats_new_layer, whats_new_buffer);

  // 

}

static void whats_new_window_unload(Window *window) {
  // Destroy TextLayer
  text_layer_destroy(s_whats_new_layer);

  // start splash timer
  s_splash_timer = app_timer_register(2 * 1000, (AppTimerCallback) splash_timer_callback, NULL);
}


static void init() {

  // used to determine whether to display what's new
  bool is_new_version = false;

  // get initial storage version, or set to 0 if none
  s_storage_version_code = persist_exists(STORAGE_VERSION_CODE_KEY) ? persist_read_int(STORAGE_VERSION_CODE_KEY) : 0;

  // no special handling required for this one (unlike version) - if it's not set, just default to Bright Green

  if (persist_exists(STORAGE_FOREGROUND_COLOR_KEY)) {
#ifdef PBL_COLOR
    s_foreground_color.argb = persist_read_int(STORAGE_FOREGROUND_COLOR_KEY);
#else
    s_foreground_color.argb = persist_read_int(STORAGE_FOREGROUND_COLOR_KEY);
#endif
  } else {
#ifdef PBL_COLOR
    s_foreground_color = GColorBrightGreen;
#else
    s_foreground_color = GColorWhite;
#endif
  }

  

  // compare storage version to current version code
  if (s_storage_version_code < VERSION_CODE) {
    // storage is old

    // for now we use this to display "what's new" message
    is_new_version = true;

    // 1.9 -> 1.10 migration (using s_foreground_color.argb now vs. s_foreground_color set to GColor*ARGB8 values)
    if (s_storage_version_code == 109) {
      // TODO: some kind of conversion from GColor*ARGB8 value???

      // actually, for now - let's just reinitialize, it's not that big of a deal!?
#ifdef PBL_COLOR
      s_foreground_color = GColorBrightGreen;
#else
      s_foreground_color = GColorWhite;
#endif

    }
    // TODO: initialize if 0?

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
    s_splash_timer = app_timer_register(2 * 1000, (AppTimerCallback) splash_timer_callback, NULL);
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

  // persist storage version and color between launches
  persist_write_int(STORAGE_VERSION_CODE_KEY, s_storage_version_code);
  persist_write_int(STORAGE_FOREGROUND_COLOR_KEY, s_foreground_color.argb);

  // Destroy Windows
  window_destroy(s_main_window);
  window_destroy(s_whats_new_window);
}

int main(void) {
  init();
  app_event_loop();
  deinit();
}