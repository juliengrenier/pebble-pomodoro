#include "pebble_os.h"
#include "pebble_app.h"
#include "pebble_fonts.h"


#define MY_UUID { 0x34, 0x25, 0x24, 0xB8, 0xB4, 0x01, 0x44, 0x55, 0x89, 0x8B, 0x0C, 0xB6, 0x25, 0xFF, 0x62, 0x31 }
PBL_APP_INFO(MY_UUID,
    "Pomodoro App", "mail@juliengrenier.cc",
    1, 0, /* App version */
    RESOURCE_ID_IMAGE_MENU_ICON,
    APP_INFO_STANDARD_APP);

Window window;
BmpContainer background_image_container;
TextLayer title_layer;
TextLayer remaining_text_layer;
AppContextRef app;


#ifdef DEBUG
#define TIMER_DELAY 1000
#else
#define TIMER_DELAY 60000
#endif

#ifndef APP_TIMER_INVALID_HANDLE
#define APP_TIMER_INVALID_HANDLE 0xDEADBEEF
#endif
#define TIMER_UPDATE 1
//#define PERIOD_OVER  2
#define POMODORO_SET_LENGTH 4
#define NUM_WORK_SETTINGS 5
#define NUM_RELAX_SETTINGS 6
#define BUTTON_WORKTIMER BUTTON_ID_UP
#define BUTTON_RUN BUTTON_ID_SELECT
#define BUTTON_RELAXTIMER BUTTON_ID_DOWN
static bool started=false;
static bool working_mode=true;
static AppTimerHandle update_timer = APP_TIMER_INVALID_HANDLE;
static const int work_lenghts[NUM_WORK_SETTINGS] = { 15, 20, 25, 30, 35 }; 
static const char relax_lenghts[NUM_RELAX_SETTINGS] = { 3, 4, 5, 6, 7, 8 }; 
static int current_work_lenght_index = 0;
static int current_relax_lenght_index = 0;
static int remaining_time = -1; 
static char remaining_time_text[2] = "00";

void reset_work_count_down();
void reset_relax_count_down();
void stop_timer();
void start_timer();
void reset_timer();
void config_provider(ClickConfig **config, Window *window); 
void stop_timer_handler(ClickRecognizerRef recognizer, Window *window);
void work_elapse_timer_handler(ClickRecognizerRef recognizer, Window *window);
void relax_elapse_timer_handler(ClickRecognizerRef recognizer, Window *window);
void toggle_timer(ClickRecognizerRef recognizer, Window *window);

void itoa2(int num, char* buffer) {
  const char digits[10] = "0123456789";
  if(num > 99) {
    buffer[0] = '9';
    buffer[1] = '9';
    return;
  } else if(num > 9) {
    buffer[0] = digits[num / 10];
  } else {
    buffer[0] = '0';
  }
  buffer[1] = digits[num % 10];
}

int get_current_work_length(){
  return work_lenghts[current_work_lenght_index];
}

int get_current_relax_length(){
  return relax_lenghts[current_relax_lenght_index];
}


void handle_deinit(AppContextRef ctx) {
  (void)ctx;
  bmp_deinit_container(&background_image_container);
}
void handle_init(AppContextRef ctx) {
  app = ctx;

  window_init(&window, "Pomodoro");
  window_stack_push(&window, true /* Animated */);
  window_set_background_color(&window, GColorBlack);
  window_set_fullscreen(&window, false);
  Layer *root_layer = window_get_root_layer(&window);
  // Set up a layer for the static watch face background
  resource_init_current_app(&APP_RESOURCES);
  bmp_init_container(RESOURCE_ID_IMAGE_BACKGROUND, &background_image_container);
  layer_add_child(root_layer, &background_image_container.layer.layer);


  text_layer_init(&title_layer, GRect(55, 50, 55, 35));
  text_layer_set_background_color(&title_layer, GColorBlack);
  text_layer_set_text_color(&title_layer, GColorWhite);

  text_layer_init(&remaining_text_layer, GRect(55, 80, 25, 15));
  text_layer_set_background_color(&remaining_text_layer, GColorBlack);
  text_layer_set_text_color(&remaining_text_layer, GColorWhite);

  layer_add_child(root_layer, &title_layer.layer);
  layer_add_child(root_layer, &remaining_text_layer.layer);
  reset_work_count_down();

  // Arrange for user input.
  window_set_click_config_provider(&window, (ClickConfigProvider) config_provider);
}
void handle_timer(AppContextRef ctx, AppTimerHandle handle, uint32_t cookie) {
  (void)handle;
  if(cookie == TIMER_UPDATE) {
    if(started) {
      remaining_time -= 1;
      if (remaining_time < 0){
        if (working_mode){
          vibes_short_pulse();
          reset_relax_count_down();
        }
        else{
          vibes_double_pulse();
          reset_work_count_down();
        }
      }else{
        itoa2(remaining_time, remaining_time_text);
        text_layer_set_text(&remaining_text_layer, remaining_time_text);
      }
      
      start_timer();
    }
  }
}
void start_timer(){
  update_timer = app_timer_send_event(app, TIMER_DELAY, TIMER_UPDATE);
  started = true;
}

void stop_timer(){
  started = false;
  if (update_timer != APP_TIMER_INVALID_HANDLE){
    if(app_timer_cancel_event(app, update_timer)) {
      update_timer = APP_TIMER_INVALID_HANDLE;
    }
  }
}
void reset_work_count_down(){
  remaining_time = get_current_work_length();
  itoa2(remaining_time, remaining_time_text);
  text_layer_set_text(&remaining_text_layer, remaining_time_text);
  text_layer_set_text(&title_layer, "WORK");
  working_mode = true;
}
void reset_relax_count_down(){
  remaining_time = get_current_relax_length();
  itoa2(remaining_time, remaining_time_text);
  text_layer_set_text(&remaining_text_layer, remaining_time_text);
  text_layer_set_text(&title_layer, "RELAX");
  working_mode = false;
}
void config_provider(ClickConfig **config, Window *window) {
  config[BUTTON_WORKTIMER]->click.handler = (ClickHandler)work_elapse_timer_handler;
  config[BUTTON_RUN]->click.handler = (ClickHandler)toggle_timer;
  config[BUTTON_RUN]->long_click.handler = (ClickHandler) stop_timer_handler;
  config[BUTTON_RUN]->long_click.release_handler = (ClickHandler) reset_timer;
  config[BUTTON_RUN]->long_click.delay_ms = 700;
  config[BUTTON_RELAXTIMER]->click.handler = (ClickHandler)relax_elapse_timer_handler;
  (void)window;
}

void stop_timer_handler(ClickRecognizerRef recognizer, Window *window){
  (void)window;
  stop_timer();
}
void work_elapse_timer_handler(ClickRecognizerRef recognizer, Window *window){
  if (started){
    return;
  }
  current_work_lenght_index += 1;
  if ( current_work_lenght_index == NUM_WORK_SETTINGS){
    current_work_lenght_index = 0;
  }
}
void relax_elapse_timer_handler(ClickRecognizerRef recognizer, Window *window){
  if (started){
    return;
  }
  current_relax_lenght_index += 1;
  if ( current_relax_lenght_index == NUM_RELAX_SETTINGS){
    current_relax_lenght_index = 0;
  }
}
void reset_timer(ClickRecognizerRef recognizer, Window *window){
  stop_timer();
  reset_work_count_down();
}
void toggle_timer(ClickRecognizerRef recognizer, Window *window){
  if (update_timer == APP_TIMER_INVALID_HANDLE){
    start_timer();
  }else{
    stop_timer();
  }
}

void pbl_main(void *params) {
  PebbleAppHandlers handlers = {
    .init_handler = &handle_init,
    .deinit_handler = &handle_deinit,
    .timer_handler = &handle_timer
  };
  app_event_loop(params, &handlers);
}
