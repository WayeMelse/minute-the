#include <pebble.h>

static Window *window;
static Window *timer_window;
static MenuLayer *menu_layer;
static TextLayer *name_layer;
static Layer *progress_layer;
static Layer *cup_layer;
static AppTimer *steam_timer;
static bool steam_running;
static uint8_t steam_phase;
static Layer *separator_layer;
static TextLayer *clock_layer;
static TextLayer *duration_layer;
static TextLayer *zero_layer;
static char clock_buf[12];
static char duration_buf[16];
static time_t start_s;
static int prev_s;

static int num_entries;
static int selected_entry;
static int bar_width;
static int bar_total;

static int saved_current;


#define STORAGE_VERSION 2

#define STORE_BASE 123400000
#define STORE_VERSION 1 + STORE_BASE
#define STORE_CURRENT 2 + STORE_BASE
#define STORE_SIZE 3 + STORE_BASE
#define STORE_ENTRY 4 + STORE_BASE

#define PROGRESS_MARGIN 3
#define MAX_ENTRIES 15
#define STORE_THEME (STORE_BASE + 100)

static bool light_theme;

static GColor theme_background(void) {
  return light_theme ? GColorWhite : GColorBlack;
}

static GColor theme_foreground(void) {
  return light_theme ? GColorBlack : GColorWhite;
}

typedef struct tea_entry {
  uint8_t quarts_low;
  uint8_t quarts_high;
  char name[20];
  char content[30];
} __attribute__((__packed__)) tea_entry;

tea_entry entries[MAX_ENTRIES];

#define STORE_COLOR_BASE (STORE_BASE + 200)
static uint8_t tea_colors[MAX_ENTRIES];

static GColor tea_accent(void)
{
  switch(tea_colors[selected_entry]) {
    case 0: return GColorWhite;
    case 2: return GColorBlack;
    case 3: return GColorOrange;
    case 4: return GColorYellow;
    default: return GColorBrightGreen;
  }
}

static GColor tea_clock_foreground(void)
{
  return tea_colors[selected_entry] == 2 ? GColorWhite : GColorBlack;
}

static GColor tea_progress_color(void)
{
  uint8_t color = tea_colors[selected_entry];
  if((color == 0 && light_theme) || (color == 2 && !light_theme)) {
    return theme_foreground();
  }
  return tea_accent();
}

static void update_tea_colors(void)
{
  if(clock_layer) {
    text_layer_set_background_color(clock_layer, tea_accent());
    text_layer_set_text_color(clock_layer, tea_clock_foreground());
  }
  if(progress_layer) {
    layer_mark_dirty(progress_layer);
  }
  if(cup_layer) {
    layer_mark_dirty(cup_layer);
  }
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu, uint16_t section_index, void *data)
{
  return num_entries;
}

static int16_t menu_get_cell_height_callback(
    MenuLayer *menu, MenuIndex *cell_index, void *data)
{
  return 70;
}

static void menu_draw_row_callback(GContext *ctx, const Layer *cell_layer,
                                   MenuIndex *cell_index, void *data)
{
  tea_entry *t = entries + cell_index->row;
  GRect bounds = layer_get_bounds(cell_layer);
  bool selected = menu_cell_layer_is_highlighted(cell_layer);

  graphics_context_set_fill_color(ctx,
    selected ? theme_foreground() : theme_background());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx,
    selected ? theme_background() : theme_foreground());

  graphics_draw_text(ctx, t->name,
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
    GRect(8, 0, bounds.size.w - 16, 35),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);

  graphics_draw_text(ctx, t->content,
    fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(8, 34, bounds.size.w - 16, 32),
    GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft, NULL);
}

static void menu_select_callback(MenuLayer *menu, MenuIndex *cell_index, void *data)
{
  selected_entry = cell_index->row;
  window_stack_push(timer_window, true);
}

static void progress_paint_callback(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);

  int h = bounds.size.h - 2 * PROGRESS_MARGIN;
  int mark_low = entries[selected_entry].quarts_low;
  int mark_high = entries[selected_entry].quarts_high;

  /* Bordure pleine : épaisseur déterministe de 3 pixels. */
  graphics_context_set_fill_color(ctx, theme_foreground());
  graphics_fill_rect(ctx, bounds, 5, GCornersAll);
  graphics_context_set_fill_color(ctx, theme_background());
  graphics_fill_rect(ctx,
    GRect(PROGRESS_MARGIN, PROGRESS_MARGIN, bar_total, h),
    2, GCornersAll);


  graphics_context_set_fill_color(ctx, tea_progress_color());
  if(bar_width > 0) {
    graphics_fill_rect(ctx,
      GRect(PROGRESS_MARGIN, PROGRESS_MARGIN, bar_width, h),
      2, GCornersAll);
  }

  graphics_context_set_stroke_color(ctx, theme_foreground());
  graphics_context_set_stroke_width(ctx, 1);
  for(int i = 1; i < mark_high; ++i) {
    int x = PROGRESS_MARGIN + (bar_total * i) / mark_high;
    if(x >= PROGRESS_MARGIN + bar_width) {
      int inset = (i == mark_low) ? 4 : ((i % 4) ? 10 : 7);
      graphics_draw_line(ctx,
        GPoint(x, inset),
        GPoint(x, bounds.size.h - inset - 1));
    }
  }
}

static void separator_paint_callback(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, theme_foreground());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void cup_draw_outline(GContext *ctx, int cx,
                             const GPoint *points, size_t count)
{
  for(size_t i = 1; i < count; ++i) {
    graphics_draw_line(ctx,
      GPoint(cx + points[i - 1].x, points[i - 1].y),
      GPoint(cx + points[i].x, points[i].y));
  }
}

static void cup_fill_shape(GContext *ctx, int cx,
                           GPoint *points, uint32_t count)
{
  GPathInfo info = {
    .num_points = count,
    .points = points
  };
  GPath *path = gpath_create(&info);
  if(!path) {
    return;
  }
  gpath_move_to(path, GPoint(cx, 0));
  gpath_draw_filled(ctx, path);
  gpath_destroy(path);
}

static void steam_stop(void)
{
  steam_running = false;
  if(steam_timer) {
    app_timer_cancel(steam_timer);
    steam_timer = NULL;
  }
}

static void steam_timer_callback(void *context)
{
  steam_timer = NULL;
  if(!steam_running || !cup_layer) {
    return;
  }

  steam_phase = (steam_phase + 1) % 24;
  layer_mark_dirty(cup_layer);
  steam_timer = app_timer_register(125, steam_timer_callback, NULL);
}

static void steam_start(void)
{
  steam_stop();
  steam_phase = 0;
  steam_running = true;
  if(cup_layer) {
    layer_mark_dirty(cup_layer);
  }
  steam_timer = app_timer_register(125, steam_timer_callback, NULL);
}

static void steam_draw(GContext *ctx, int cx,
                       const GPoint *points, size_t count,
                       uint8_t offset)
{
  if(count < 2) {
    return;
  }

  static const int8_t wave[] = {
     0,  1,  1,  1,  2,  2,  2,  2,
     2,  1,  1,  1,  0, -1, -1, -1,
    -2, -2, -2, -2, -2, -1, -1, -1
  };

  int min_x = points[0].x;
  int max_x = points[0].x;
  int min_y = points[0].y;
  int max_y = points[0].y;

  for(size_t i = 1; i < count; ++i) {
    if(points[i].x < min_x) min_x = points[i].x;
    if(points[i].x > max_x) max_x = points[i].x;
    if(points[i].y < min_y) min_y = points[i].y;
    if(points[i].y > max_y) max_y = points[i].y;
  }

  int center_x = cx + (min_x + max_x) / 2;

  for(int y = min_y + 1; y <= max_y; ++y) {
    int phase0 = (y - 1 + steam_phase + offset) % 24;
    int phase1 = (y + steam_phase + offset) % 24;

    bool thin_tip = (y <= min_y + 3) || (y > max_y - 3);
    graphics_context_set_stroke_width(ctx, thin_tip ? 1 : 2);

    graphics_draw_line(ctx,
      GPoint(center_x + wave[phase0], y - 1),
      GPoint(center_x + wave[phase1], y));
  }

  graphics_context_set_stroke_width(ctx, 2);
}

static void cup_paint_callback(Layer *layer, GContext *ctx)
{
  int cx = layer_get_bounds(layer).size.w / 2;

  static const GPoint rim[] = {
    {-26, 33}, {-24, 31}, {-19, 29}, {-10, 28},
    {10, 28}, {19, 29}, {24, 31}, {26, 33},
    {24, 35}, {19, 37}, {10, 38}, {-10, 38},
    {-19, 37}, {-24, 35}, {-26, 33}
  };

  static const GPoint body[] = {
    {-26, 33}, {-25, 39}, {-23, 45}, {-20, 50},
    {-16, 54}, {-11, 57}, {-5, 58},
    {5, 58}, {11, 57}, {16, 54},
    {20, 50}, {23, 45}, {25, 39}, {26, 33}
  };

  static const GPoint handle[] = {
    {26, 35}, {31, 35}, {35, 37}, {37, 40},
    {37, 43}, {35, 46}, {31, 48}, {22, 49}
  };

  static const GPoint saucer[] = {
    {-20, 53}, {-27, 54}, {-33, 56}, {-36, 59},
    {-34, 61}, {-28, 63}, {-18, 65}, {-8, 66},
    {8, 66}, {18, 65}, {28, 63}, {34, 61},
    {36, 59}, {33, 56}, {27, 54}, {20, 53}
  };

  static GPoint saucer_band[] = {
    {-36, 59}, {-34, 61}, {-28, 63}, {-18, 65},
    {-8, 66}, {8, 66}, {18, 65}, {28, 63},
    {34, 61}, {36, 59},
    {29, 60}, {19, 62}, {8, 63}, {-8, 63},
    {-19, 62}, {-29, 60}
  };

  static GPoint liquid[] = {
    {-26, 33}, {-24, 31}, {-19, 29}, {-10, 28},
    {10, 28}, {19, 29}, {24, 31}, {26, 33},
    {24, 35}, {19, 37}, {10, 38}, {-10, 38},
    {-19, 37}, {-24, 35}
  };

  static const GPoint steam_left[] = {
    {-15, 24}, {-12, 20}, {-12, 16},
    {-16, 12}, {-16, 8}, {-13, 5}
  };

  static const GPoint steam_middle[] = {
    {0, 22}, {3, 18}, {3, 13},
    {-1, 9}, {-1, 4}, {2, 1}
  };

  static const GPoint steam_right[] = {
    {15, 24}, {18, 20}, {18, 16},
    {14, 12}, {14, 8}, {17, 5}
  };

  graphics_context_set_stroke_color(ctx, theme_foreground());
  graphics_context_set_stroke_width(ctx, 2);

  graphics_context_set_fill_color(ctx, theme_foreground());
  cup_fill_shape(ctx, cx, saucer_band,
    sizeof(saucer_band) / sizeof(saucer_band[0]));

  bool finished = bar_total > 0 && bar_width >= bar_total;
  uint8_t color_id = tea_colors[selected_entry];
  GColor liquid_color = (color_id == 0 || color_id == 2)
    ? GColorPastelYellow : tea_accent();

  graphics_context_set_fill_color(ctx,
    finished ? liquid_color : theme_background());
  cup_fill_shape(ctx, cx, liquid,
    sizeof(liquid) / sizeof(liquid[0]));

  cup_draw_outline(ctx, cx, saucer,
    sizeof(saucer) / sizeof(saucer[0]));
  cup_draw_outline(ctx, cx, handle,
    sizeof(handle) / sizeof(handle[0]));
  cup_draw_outline(ctx, cx, body,
    sizeof(body) / sizeof(body[0]));
  cup_draw_outline(ctx, cx, rim,
    sizeof(rim) / sizeof(rim[0]));

  steam_draw(ctx, cx, steam_left,
    sizeof(steam_left) / sizeof(steam_left[0]), 0);
  steam_draw(ctx, cx, steam_middle,
    sizeof(steam_middle) / sizeof(steam_middle[0]), 4);
  steam_draw(ctx, cx, steam_right,
    sizeof(steam_right) / sizeof(steam_right[0]), 8);
}

static void tick_handler(struct tm *tick_time, TimeUnits units_changed)
{
  time_t now = time(NULL);
  struct tm *local = localtime(&now);
  strftime(clock_buf, sizeof(clock_buf),
           clock_is_24h_style() ? "%H:%M" : "%I:%M", local);
  text_layer_set_text(clock_layer, clock_buf);

  int total_s = entries[selected_entry].quarts_high * 15;
  int low_s = entries[selected_entry].quarts_low * 15;
  if(total_s <= 0) {
    total_s = 15;
  }

  int elapsed_s = (int)(now - start_s);
  if(elapsed_s < 0) {
    elapsed_s = 0;
  }
  if(elapsed_s > total_s) {
    elapsed_s = total_s;
  }

  bar_width = (bar_total * elapsed_s) / total_s;

  /* Une seule vibration de fin ; l'horloge continue ensuite. */
  if(elapsed_s >= total_s) {
    if(prev_s < total_s) {
      layer_mark_dirty(cup_layer);
      static const uint32_t segments[] = {200, 150, 200};
      VibePattern pattern = {
        .durations = segments,
        .num_segments = 3
      };
      vibes_enqueue_custom_pattern(pattern);
    }
  } else if(prev_s >= 0 && prev_s < low_s && elapsed_s >= low_s) {
    vibes_short_pulse();
  }

  prev_s = elapsed_s;
  layer_mark_dirty(progress_layer);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);

  menu_layer = menu_layer_create(bounds);
  menu_layer_set_normal_colors(menu_layer,
    theme_background(), theme_foreground());
  menu_layer_set_highlight_colors(menu_layer,
    theme_foreground(), theme_background());

  menu_layer_set_callbacks(menu_layer, 0, (MenuLayerCallbacks){
      .get_num_rows = menu_get_num_rows_callback,
        .get_cell_height = menu_get_cell_height_callback,
        .draw_row = menu_draw_row_callback,
        .select_click = menu_select_callback
    });

  menu_layer_set_click_config_onto_window(menu_layer, window);
  layer_add_child(window_layer, menu_layer_get_layer(menu_layer));
}

static void window_unload(Window *window) {
  menu_layer_destroy(menu_layer);
}

static void timer_window_load(Window *window) {
  window_set_background_color(window, theme_background());
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;
  int h = bounds.size.h;

  /* Bandeau de 56 px : moins du quart de l'écran Emery. */
  clock_layer = text_layer_create(GRect(0, 0, w, 56));
  text_layer_set_font(clock_layer,
                     fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD));
  text_layer_set_text_alignment(clock_layer, GTextAlignmentCenter);
  text_layer_set_background_color(clock_layer, tea_accent());
  text_layer_set_text_color(clock_layer, tea_clock_foreground());
  layer_add_child(root, text_layer_get_layer(clock_layer));

  separator_layer = layer_create(GRect(0, 56, w, 2));
  layer_set_update_proc(separator_layer, separator_paint_callback);
  layer_add_child(root, separator_layer);

  name_layer = text_layer_create(GRect(4, 59, w - 8, 37));
  text_layer_set_font(name_layer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
  text_layer_set_text_alignment(name_layer, GTextAlignmentCenter);
  text_layer_set_overflow_mode(name_layer, GTextOverflowModeTrailingEllipsis);
  text_layer_set_background_color(name_layer, GColorClear);
  text_layer_set_text_color(name_layer, theme_foreground());
  layer_add_child(root, text_layer_get_layer(name_layer));

  cup_layer = layer_create(GRect(0, 94, w, 68));
  layer_set_update_proc(cup_layer, cup_paint_callback);
  layer_add_child(root, cup_layer);

  bar_total = w - 24 - 2 * PROGRESS_MARGIN;
  progress_layer = layer_create(GRect(12, h - 60, w - 24, 28));
  layer_set_update_proc(progress_layer, progress_paint_callback);
  layer_add_child(root, progress_layer);

  duration_layer = text_layer_create(GRect(w / 2, h - 33, w / 2 - 12, 32));
  text_layer_set_font(duration_layer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(duration_layer, GTextAlignmentRight);
  text_layer_set_background_color(duration_layer, GColorClear);
  text_layer_set_text_color(duration_layer, theme_foreground());
  layer_add_child(root, text_layer_get_layer(duration_layer));

  zero_layer = text_layer_create(GRect(12, h - 33, 40, 32));
  text_layer_set_font(zero_layer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(zero_layer, GTextAlignmentLeft);
  text_layer_set_background_color(zero_layer, GColorClear);
  text_layer_set_text_color(zero_layer, theme_foreground());
  text_layer_set_text(zero_layer, "0");
  layer_add_child(root, text_layer_get_layer(zero_layer));
}

static void timer_window_appear(Window *window) {
  tea_entry *entry = entries + selected_entry;
  text_layer_set_text(name_layer, entry->name);
  update_tea_colors();

  int total_s = entry->quarts_high * 15;
  if(total_s % 60 == 0) {
    snprintf(duration_buf, sizeof(duration_buf), "%d min", total_s / 60);
  } else {
    snprintf(duration_buf, sizeof(duration_buf),
             "%d:%02d", total_s / 60, total_s % 60);
  }
  text_layer_set_text(duration_layer, duration_buf);

  bar_width = 0;
  start_s = time(NULL);
  prev_s = -1;
  tick_handler(NULL, SECOND_UNIT);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  steam_start();
}

static void timer_window_disappear(Window *window) {
  steam_stop();
  tick_timer_service_unsubscribe();
}

static void timer_window_unload(Window *window) {
  steam_stop();
  tick_timer_service_unsubscribe();
  text_layer_destroy(name_layer);
  text_layer_destroy(clock_layer);
  text_layer_destroy(duration_layer);
  text_layer_destroy(zero_layer);
  layer_destroy(progress_layer);
  layer_destroy(cup_layer);
  layer_destroy(separator_layer);
  separator_layer = NULL;
  clock_layer = NULL;
  name_layer = NULL;
  duration_layer = NULL;
  zero_layer = NULL;
  cup_layer = NULL;
  progress_layer = NULL;
}

static void apply_theme(void)
{
  if(menu_layer) {
    menu_layer_set_normal_colors(menu_layer,
      theme_background(), theme_foreground());
    menu_layer_set_highlight_colors(menu_layer,
      theme_foreground(), theme_background());
    layer_mark_dirty(menu_layer_get_layer(menu_layer));
  }

  if(timer_window && clock_layer) {
    window_set_background_color(timer_window, theme_background());
    text_layer_set_text_color(name_layer, theme_foreground());
    text_layer_set_text_color(duration_layer, theme_foreground());
    text_layer_set_text_color(zero_layer, theme_foreground());
    layer_mark_dirty(separator_layer);
    layer_mark_dirty(cup_layer);
    layer_mark_dirty(progress_layer);
  }
}

static void message_received(DictionaryIterator *iter, void *context)
{
  Tuple *tp = dict_find(iter, 0);
  if(!tp || tp->type != TUPLE_CSTRING) {
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Invalid message");
    return;
  }
  if(strcmp(tp->value->cstring, "t") == 0) {
    Tuple *theme = dict_find(iter, 8);
    if(theme) {
      light_theme = theme->value->int32 != 0;
      persist_write_int(STORE_THEME, light_theme ? 1 : 0);
      apply_theme();
    }
    return;
  }

  if(tp->value->cstring[0] == 'u') {
    /* Single config entry */
    int idx;
    int count;

    tp = dict_find(iter, 1);
    if(!tp) { return; }
    idx = tp->value->int32;

    if(idx < 0 || idx >= MAX_ENTRIES) {
      return;
    }

    tp = dict_find(iter, 2);
    if(!tp) { return; }
    count = tp->value->int32;
    if(count < 1 || count > MAX_ENTRIES || idx >= count) {
      return;
    }

    tp = dict_find(iter, 3);
    if(!tp) { return; }
    strncpy(entries[idx].name, tp->value->cstring, sizeof(entries[0].name));
    entries[idx].name[sizeof(entries[idx].name) - 1] = '\0';

    tp = dict_find(iter, 4);
    if(!tp) { return; }
    strncpy(entries[idx].content, tp->value->cstring, sizeof(entries[0].content));
    entries[idx].content[sizeof(entries[idx].content) - 1] = '\0';

    tp = dict_find(iter, 5);
    if(!tp) { return; }
    int lo = tp->value->int32;
    
    tp = dict_find(iter, 6);
    if(!tp) { return; }
    int hi = tp->value->int32;
    if(lo > hi) {
      hi = lo;
    }
    entries[idx].quarts_low = lo;
    entries[idx].quarts_high = hi;

    Tuple *color = dict_find(iter, 7);
    int color_id = color ? color->value->int32 : 1;
    if(color_id < 0 || color_id > 4) {
      color_id = 1;
    }
    tea_colors[idx] = color_id;
    persist_write_int(STORE_COLOR_BASE + idx, color_id);
    if(idx == selected_entry) {
      update_tea_colors();
    }

    if(idx >= num_entries || idx+1 == count) {
      num_entries = idx + 1;
    }

    /* Le téléphone envoie les thés dans l'ordre, un par message. */
    if(idx + 1 == count) {
      bool saved = true;
      for(int i = 0; i < count; ++i) {
        int written = persist_write_data(
          STORE_ENTRY + i, &entries[i], sizeof(tea_entry));
        if(written != (int)sizeof(tea_entry)) {
          saved = false;
          APP_LOG(APP_LOG_LEVEL_ERROR,
            "Tea storage failed: %d (%d)", i, written);
        }
      }

      if(saved) {
        int written = persist_write_int(STORE_SIZE, count);
        if(written != (int)sizeof(int32_t)) {
          APP_LOG(APP_LOG_LEVEL_ERROR,
            "Tea count storage failed: %d", written);
        }
      }
    }

    menu_layer_reload_data(menu_layer);
    if(saved_current >=0 && saved_current < num_entries) {
      menu_layer_set_selected_index(menu_layer, (MenuIndex){.row=saved_current, .section=0}, MenuRowAlignCenter, false);
    }
    layer_mark_dirty(menu_layer_get_layer(menu_layer));
  }
}

static void message_dropped(AppMessageResult reason, void *context)
{
  /*  APP_LOG(APP_LOG_LEVEL_DEBUG, "Message dropped %d", reason); */
}

static void init(void) {
  light_theme = persist_exists(STORE_THEME)
    && persist_read_int(STORE_THEME) != 0;
  num_entries = 0;
  for(int i=0; i<MAX_ENTRIES; ++i) {
    strcpy(entries[i].name, "Default tea");
    strcpy(entries[i].content, "Make in the usual way");
    entries[i].quarts_low = 12;
    entries[i].quarts_high = 20;
    int stored_color = persist_exists(STORE_COLOR_BASE + i)
      ? persist_read_int(STORE_COLOR_BASE + i) : 1;
    tea_colors[i] = (stored_color >= 0 && stored_color <= 4)
      ? stored_color : 1;
  }

  app_message_register_inbox_received(message_received);
  app_message_register_inbox_dropped(message_dropped);
  app_message_open(app_message_inbox_size_maximum(), 100);

  int ret;
  if(!persist_exists(STORE_VERSION) ||
     STORAGE_VERSION != persist_read_int(STORE_VERSION)) {
    // FIXME - pull from app, instead
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Setting app config");
    ret = persist_write_int(STORE_VERSION, STORAGE_VERSION);
    APP_LOG(APP_LOG_LEVEL_DEBUG, "STORE_VERSION: %d", ret);
    ret = persist_write_int(STORE_SIZE, 0);
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_cstring(iter, 0, "c");
    app_message_outbox_send();
  }
  num_entries = persist_read_int(STORE_SIZE);
  if(num_entries < 0 || num_entries > MAX_ENTRIES) {
    num_entries = 0;
  }

  for(int i = 0; i < num_entries; ++i) {
    int bytes = persist_read_data(
      STORE_ENTRY + i, &entries[i], sizeof(tea_entry));
    if(bytes != (int)sizeof(tea_entry)) {
      num_entries = 0;
      break;
    }
    entries[i].name[sizeof(entries[i].name) - 1] = '\0';
    entries[i].content[sizeof(entries[i].content) - 1] = '\0';
  }
  if(num_entries == 0) {
    num_entries = 1;
  }

  saved_current = -1;
  if(persist_exists(STORE_CURRENT)) {
    saved_current = persist_read_int(STORE_CURRENT);
  }
  /*  APP_LOG(APP_LOG_LEVEL_DEBUG, "Reading config from phone"); */
  
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
      .load = window_load,
        .unload = window_unload,
  });

  timer_window = window_create();
  window_set_window_handlers(timer_window, (WindowHandlers) {
      .load = timer_window_load,
        .appear = timer_window_appear,
        .disappear = timer_window_disappear,
        .unload = timer_window_unload,
  });

  window_stack_push(window, true);
}

static void deinit(void) {
  window_stack_pop_all(false);
  window_destroy(window);
  window_destroy(timer_window);
  persist_write_int(STORE_CURRENT, selected_entry);
}

int main(void) {
  init();


  app_event_loop();
  deinit();
}
