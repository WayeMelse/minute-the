#include <pebble.h>

static Window *window;
static Window *timer_window;
static Window *confirm_window;
static TextLayer *confirm_stop_layer;
static TextLayer *confirm_continue_layer;
static Layer *confirm_bar_layer;
static Layer *confirm_cup_layer;
static bool confirm_open;
static MenuLayer *menu_layer;
static TextLayer *name_layer;
static Layer *progress_layer;
static Layer *cup_layer;
static AppTimer *steam_timer;
static bool steam_running;
static uint8_t steam_phase;
static Layer *separator_layer;
static TextLayer *clock_layer;
#ifdef PBL_ROUND
static Layer *clock_top_layer;
#endif
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
#define STORE_SOUND (STORE_BASE + 101)
static bool sound_enabled;

#define STORE_SESSION (STORE_BASE + 102)

typedef struct __attribute__((__packed__)) brew_session {
  uint8_t version;
  uint8_t tea_index;
  time_t start_s;
} brew_session;

static void save_session(void)
{
  brew_session session = {
    .version = 1,
    .tea_index = (uint8_t)selected_entry,
    .start_s = start_s
  };
  persist_write_data(STORE_SESSION, &session, sizeof(session));
}

static bool load_session(void)
{
  brew_session session;

  if(persist_get_size(STORE_SESSION) != (int)sizeof(session)) {
    return false;
  }
  if(persist_read_data(STORE_SESSION, &session, sizeof(session))
      != (int)sizeof(session)) {
    return false;
  }
  if(session.version != 1 || session.tea_index != selected_entry) {
    return false;
  }

  start_s = session.start_s;
  return true;
}

static void clear_session(void)
{
  persist_delete(STORE_SESSION);
}

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

#ifdef PBL_ROUND
static void clock_top_paint_callback(Layer *layer, GContext *ctx)
{
  graphics_context_set_fill_color(ctx, tea_accent());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}
#endif

static void update_tea_colors(void)
{
#ifdef PBL_ROUND
  if(clock_top_layer) {
    layer_mark_dirty(clock_top_layer);
  }
#endif
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

#ifdef PBL_ROUND
  const int text_margin = 40;
  const GTextAlignment text_alignment = GTextAlignmentCenter;
#else
  const int text_margin = 8;
  const GTextAlignment text_alignment = GTextAlignmentLeft;
#endif

  graphics_context_set_fill_color(ctx,
    selected ? theme_foreground() : theme_background());
  graphics_fill_rect(ctx, bounds, 0, GCornerNone);
  graphics_context_set_text_color(ctx,
    selected ? theme_background() : theme_foreground());

  graphics_draw_text(ctx, t->name,
    fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
    GRect(text_margin, 0, bounds.size.w - 2 * text_margin, 35),
    GTextOverflowModeTrailingEllipsis, text_alignment, NULL);

  graphics_draw_text(ctx, t->content,
    fonts_get_system_font(FONT_KEY_GOTHIC_24),
    GRect(text_margin, 34, bounds.size.w - 2 * text_margin, 32),
    GTextOverflowModeTrailingEllipsis, text_alignment, NULL);
}

static void menu_select_callback(MenuLayer *menu, MenuIndex *cell_index, void *data)
{
  selected_entry = cell_index->row;
  start_s = time(NULL);
  save_session();

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

static GColor cup_mix_color(GColor a, GColor b, int amount) {
  int r = (((a.argb >> 4) & 3) * (3 - amount)
         + ((b.argb >> 4) & 3) * amount + 1) / 3;
  int g = (((a.argb >> 2) & 3) * (3 - amount)
         + ((b.argb >> 2) & 3) * amount + 1) / 3;
  int bl = ((a.argb & 3) * (3 - amount)
          + (b.argb & 3) * amount + 1) / 3;
  return (GColor){.argb = (uint8_t)(0xC0 | (r << 4) | (g << 2) | bl)};
}

static void cup_paint_callback(Layer *layer, GContext *ctx) {
  int cx = layer_get_bounds(layer).size.w / 2;

  static const GPoint rim[] = {
    {-29,30}, {-26,28}, {-21,26}, {-11,25},
    {11,25}, {21,26}, {26,28}, {29,30},
    {26,33}, {21,35}, {11,36}, {-11,36},
    {-21,35}, {-26,33}, {-29,30}
  };
  static const GPoint body[] = {
    {-29,30}, {-28,37}, {-25,44}, {-22,49},
    {-18,54}, {-12,57}, {-6,58}, {6,58},
    {12,57}, {18,54}, {22,49}, {25,44},
    {28,37}, {29,30}
  };
  static const GPoint handle[] = {
    {28,31}, {32,31}, {36,32}, {39,34},
    {40,37}, {39,40}, {37,43}, {34,45},
    {30,47}, {25,48}
  };
  static const GPoint saucer[] = {
    {-22,53}, {-30,54}, {-36,56}, {-40,58},
    {-37,60}, {-31,61}, {-20,62}, {-9,63},
    {9,63}, {20,62}, {31,61}, {37,60},
    {40,58}, {36,56}, {30,54}, {22,53}
  };
  static GPoint saucer_band[] = {
    {-24,62}, {24,62}, {24,64}, {18,66},
    {-18,66}, {-24,64}
  };
  static GPoint liquid_body[] = {
    {-25,35}, {25,35}, {24,43}, {21,48},
    {17,52}, {11,55}, {6,56}, {-6,56},
    {-11,55}, {-17,52}, {-21,48}, {-24,43}
  };
  static GPoint liquid_surface[] = {
    {-25,35}, {-23,33}, {-18,32}, {-9,31},
    {9,31}, {18,32}, {23,33}, {25,35},
    {23,37}, {18,38}, {9,39}, {-9,39},
    {-18,38}, {-23,37}
  };
  static GPoint liquid_shadow[] = {
    {21,40}, {24,38}, {23,44}, {20,49},
    {15,52}, {9,55}, {-3,56}, {-11,54},
    {1,54}, {11,50}, {18,46}
  };
  static const GPoint glass_reflection[] = {
    {-21,36}, {-20,41}, {-17,46}, {-13,51},
    {-10,53}
  };
  static const GPoint surface_front[] = {
    {-25,35}, {-23,37}, {-18,38}, {-9,39},
    {9,39}, {18,38}, {23,37}, {25,35}
  };
  static const GPoint steam_middle[] = {
    {0,20}, {3,17}, {3,13}, {-1,9}, {-1,5}, {2,1}
  };
  static const GPoint steam_left[] = {
    {-15,20}, {-12,17}, {-12,14}, {-16,11}, {-16,7}, {-13,5}
  };

  static const GPoint steam_right[] = {
    {15,20}, {18,17}, {18,14}, {14,11}, {14,7}, {17,5}
  };

  /* Quatre états : transparent, léger, infusé, terminé. */
  int total_s = entries[selected_entry].quarts_high * 15;
  int first_s = entries[selected_entry].quarts_low * 15;
  if(total_s <= 0) total_s = 15;

  int elapsed_s = (int)(time(NULL) - start_s);
  if(elapsed_s < 0) elapsed_s = 0;
  if(elapsed_s > total_s) elapsed_s = total_s;

  int stage = 0;
  if(elapsed_s >= total_s) {
    stage = 3;
  } else if(elapsed_s * 2 >= total_s) {
    stage = elapsed_s >= first_s ? 2 : 1;
  }

  GColor pale_yellow = (GColor){.argb = 0xFE}; /* RGB 255,255,170 */
  GColor yellow = (GColor){.argb = 0xFC};      /* RGB 255,255,0 */
  GColor amber = (GColor){.argb = 0xF8};       /* RGB 255,170,0 */
  GColor orange = (GColor){.argb = 0xF4};      /* RGB 255,85,0 */

  GColor liquid_fill = theme_background();
  uint8_t color_id = tea_colors[selected_entry];

  if(stage > 0) {
    if(color_id == 0) {
      /* Thé blanc. */
      liquid_fill = stage == 1 ? GColorWhite
                  : stage == 2 ? pale_yellow : yellow;
    } else if(color_id == 2 || color_id == 3) {
      /* Thé noir et oolong. */
      liquid_fill = stage == 1 ? yellow
                  : stage == 2 ? amber : orange;
    } else {
      /* Thé vert et tisane. */
      liquid_fill = stage == 1 ? pale_yellow
                  : stage == 2 ? yellow : amber;
    }
  }

  bool colored = liquid_fill.argb != theme_background().argb;
  GColor surface_color = colored
    ? cup_mix_color(liquid_fill, GColorWhite, 1)
    : theme_background();
  GColor shadow_color = colored
    ? cup_mix_color(liquid_fill, GColorBlack, 1)
    : theme_background();

  graphics_context_set_fill_color(ctx, theme_background());
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  graphics_context_set_fill_color(ctx, theme_foreground());
  cup_fill_shape(ctx, cx, saucer_band,
    sizeof(saucer_band) / sizeof(saucer_band[0]));

  graphics_context_set_fill_color(ctx, liquid_fill);
  cup_fill_shape(ctx, cx, liquid_body,
    sizeof(liquid_body) / sizeof(liquid_body[0]));

  if(colored) {
    graphics_context_set_fill_color(ctx, shadow_color);
    cup_fill_shape(ctx, cx, liquid_shadow,
      sizeof(liquid_shadow) / sizeof(liquid_shadow[0]));
  }

  graphics_context_set_fill_color(ctx, surface_color);
  cup_fill_shape(ctx, cx, liquid_surface,
    sizeof(liquid_surface) / sizeof(liquid_surface[0]));

  if(colored) {
    graphics_context_set_stroke_color(ctx, shadow_color);
    graphics_context_set_stroke_width(ctx, 1);
    cup_draw_outline(ctx, cx, surface_front,
      sizeof(surface_front) / sizeof(surface_front[0]));
  }

  graphics_context_set_stroke_color(ctx, theme_foreground());
  for(size_t i = 1;
      i < sizeof(glass_reflection) / sizeof(glass_reflection[0]); ++i) {
    graphics_context_set_stroke_width(ctx, i <= 2 ? 2 : 1);
    graphics_draw_line(ctx,
      GPoint(cx + glass_reflection[i - 1].x, glass_reflection[i - 1].y),
      GPoint(cx + glass_reflection[i].x, glass_reflection[i].y));
  }

  graphics_context_set_stroke_color(ctx, theme_foreground());
  graphics_context_set_stroke_width(ctx, 2);

  graphics_context_set_stroke_width(ctx, 2);
  cup_draw_outline(ctx, cx, saucer, sizeof(saucer) / sizeof(saucer[0]));

  /* Contours de la tasse et de son anse : 2 pixels. */
  graphics_context_set_stroke_width(ctx, 2);
  cup_draw_outline(ctx, cx, handle, sizeof(handle) / sizeof(handle[0]));
  cup_draw_outline(ctx, cx, body, sizeof(body) / sizeof(body[0]));
  cup_draw_outline(ctx, cx, rim, sizeof(rim) / sizeof(rim[0]));

  steam_draw(ctx, cx, steam_left, sizeof(steam_left) / sizeof(steam_left[0]), 0);
  steam_draw(ctx, cx, steam_middle, sizeof(steam_middle) / sizeof(steam_middle[0]), 4);
  steam_draw(ctx, cx, steam_right, sizeof(steam_right) / sizeof(steam_right[0]), 8);
}

static void play_alert_sound(bool final_alert) {
#ifdef PBL_SPEAKER
  if(!sound_enabled || speaker_is_muted()) {
    return;
  }

  static const SpeakerNote notes[] = {
    {
      .midi_note = 81,
      .waveform = SpeakerWaveformSine,
      .duration_ms = 150,
      .velocity = 100
    },
    {
      .midi_note = 0,
      .waveform = SpeakerWaveformSine,
      .duration_ms = 150
    },
    {
      .midi_note = 81,
      .waveform = SpeakerWaveformSine,
      .duration_ms = 150,
      .velocity = 100
    }
  };

  if(!speaker_play_notes(notes, final_alert ? 3 : 1, 40)) {
    APP_LOG(APP_LOG_LEVEL_WARNING, "Sound alert could not start");
  }
#else
  (void)final_alert;
#endif
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
      play_alert_sound(true);
    }
  } else if(prev_s >= 0 && prev_s < low_s && elapsed_s >= low_s) {
    vibes_short_pulse();
    play_alert_sound(false);
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

static void confirm_continue(void)
{
  confirm_open = false;
  window_stack_pop(true);
}

static void confirm_stop(void)
{
  /* Ferme la confirmation puis l'ecran d'infusion. */
  confirm_open = false;
  window_stack_pop(false);
  window_stack_pop(true);
}

static void confirm_back_handler(ClickRecognizerRef recognizer, void *context)
{
  confirm_stop();
}

static void confirm_down_handler(ClickRecognizerRef recognizer, void *context)
{
  confirm_continue();
}

static void confirm_click_provider(void *context)
{
  /* Select et Haut sont volontairement inactifs. */
  window_single_click_subscribe(BUTTON_ID_BACK, confirm_back_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, confirm_down_handler);
}

static void confirm_bar_paint_callback(Layer *layer, GContext *ctx)
{
  GRect bounds = layer_get_bounds(layer);
  const int band_w = 26;
#ifdef PBL_ROUND
  const int left_cx = band_w - 6;
  const int right_cx = bounds.size.w - band_w + 5;
#else
  const int left_cx = band_w / 2;
  const int right_cx = bounds.size.w - band_w / 2;
#endif

  graphics_context_set_fill_color(ctx, theme_foreground());
  graphics_fill_rect(ctx, GRect(0, 0, band_w, bounds.size.h), 0, GCornerNone);
  graphics_fill_rect(ctx,
    GRect(bounds.size.w - band_w, 0, band_w, bounds.size.h),
    0, GCornerNone);

  graphics_context_set_fill_color(ctx, theme_background());

  /* Fleche gauche, en face du bouton Retour : Stop. */
#ifdef PBL_ROUND
  const int stop_y = bounds.size.h / 3 - 12;
  const int continue_y = bounds.size.h * 2 / 3 + 12;
#else
  const int stop_y = bounds.size.h / 4 - 18;
  const int continue_y = bounds.size.h * 3 / 4 + 18;
#endif

#ifdef PBL_ROUND
  GPoint left_arrow[] = {
    {6, stop_y - 6},
    {6, stop_y + 6},
    {-6, stop_y}
  };
#else
  GPoint left_arrow[] = {
    {8, stop_y - 10},
    {8, stop_y + 10},
    {-10, stop_y}
  };
#endif
  cup_fill_shape(ctx, left_cx, left_arrow, 3);

  /* Fleche droite, en face du bouton Bas : Continue. */
#ifdef PBL_ROUND
  static GPoint right_arrow[] = {
    {-6, -16}, {-6, -4}, {6, -10}
  };
#else
  static GPoint right_arrow[] = {
    {-8, -20}, {-8, 0}, {10, -10}
  };
#endif
  GPoint right_shape[3];
  for(int i = 0; i < 3; ++i) {
    right_shape[i] = GPoint(right_arrow[i].x,
                            continue_y + 10 + right_arrow[i].y);
  }
  cup_fill_shape(ctx, right_cx, right_shape, 3);
}

static void confirm_cup_paint_callback(Layer *layer, GContext *ctx)
{
  cup_paint_callback(layer, ctx);

  int cx = layer_get_bounds(layer).size.w / 2;
  GPoint bottom_left = GPoint(cx - 38, 68);
  GPoint top_right = GPoint(cx + 34, 6);

  /* Un halo detache la barre du dessin de la tasse. */
  graphics_context_set_stroke_color(ctx, theme_background());
  graphics_context_set_stroke_width(ctx, 8);
  graphics_draw_line(ctx, bottom_left, top_right);

  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 5);
  graphics_draw_line(ctx, bottom_left, top_right);
  graphics_context_set_stroke_width(ctx, 1);
}

static void confirm_window_load(Window *window)
{
  window_set_background_color(window, theme_background());
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  const int band_w = 26;
  const int text_margin = 6;
  const int center_w = bounds.size.w - 2 * band_w - 2 * text_margin;
#ifdef PBL_ROUND
  const int stop_y = bounds.size.h / 3 - 12;
  const int continue_y = bounds.size.h * 2 / 3 + 12;
#else
  const int stop_y = bounds.size.h / 4 - 18;
  const int continue_y = bounds.size.h * 3 / 4 + 18;
#endif

  confirm_stop_layer = text_layer_create(
    GRect(band_w + text_margin, stop_y - 20, center_w, 40));
  text_layer_set_font(confirm_stop_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(confirm_stop_layer, GTextAlignmentLeft);
  text_layer_set_background_color(confirm_stop_layer, GColorClear);
  text_layer_set_text_color(confirm_stop_layer, theme_foreground());
  text_layer_set_text(confirm_stop_layer, "Stop brewing?");
  layer_add_child(root, text_layer_get_layer(confirm_stop_layer));

  confirm_continue_layer = text_layer_create(
    GRect(band_w + text_margin, continue_y - 20, center_w, 40));
  text_layer_set_font(confirm_continue_layer,
                      fonts_get_system_font(FONT_KEY_GOTHIC_28));
  text_layer_set_text_alignment(confirm_continue_layer, GTextAlignmentRight);
  text_layer_set_background_color(confirm_continue_layer, GColorClear);
  text_layer_set_text_color(confirm_continue_layer, theme_foreground());
  text_layer_set_text(confirm_continue_layer, "Continue?");
  layer_add_child(root, text_layer_get_layer(confirm_continue_layer));

  confirm_bar_layer = layer_create(bounds);
  layer_set_update_proc(confirm_bar_layer, confirm_bar_paint_callback);
  layer_add_child(root, confirm_bar_layer);

  /* Meme tasse que sur l'ecran d'infusion, centree entre les textes. */
  confirm_cup_layer = layer_create(
    GRect((bounds.size.w - 100) / 2, bounds.size.h / 2 - 34, 100, 74));
  layer_set_update_proc(confirm_cup_layer, confirm_cup_paint_callback);
  layer_add_child(root, confirm_cup_layer);
}

static void confirm_window_unload(Window *window)
{
  layer_destroy(confirm_cup_layer);
  confirm_cup_layer = NULL;
  layer_destroy(confirm_bar_layer);
  text_layer_destroy(confirm_stop_layer);
  text_layer_destroy(confirm_continue_layer);
  confirm_bar_layer = NULL;
  confirm_stop_layer = NULL;
  confirm_continue_layer = NULL;
}

static void timer_back_handler(ClickRecognizerRef recognizer, void *context)
{
  confirm_open = true;
  window_stack_push(confirm_window, true);
}

static void timer_click_provider(void *context)
{
  window_single_click_subscribe(BUTTON_ID_BACK, timer_back_handler);
}

static void timer_window_load(Window *window) {
  window_set_background_color(window, theme_background());
  window_set_click_config_provider(window, timer_click_provider);
  Layer *root = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(root);
  int w = bounds.size.w;
  int h = bounds.size.h;

  /* Bandeau de 56 px : moins du quart de l'écran Emery. */
#ifdef PBL_ROUND
  clock_top_layer = layer_create(GRect(0, 0, w, 4));
  layer_set_update_proc(clock_top_layer, clock_top_paint_callback);
  layer_add_child(root, clock_top_layer);

  clock_layer = text_layer_create(GRect(0, 4, w, 52));
#else
  clock_layer = text_layer_create(GRect(0, 0, w, 56));
#endif
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

#ifdef PBL_ROUND
  const int progress_margin = 40;
  const int progress_y = h - 78;
  const int label_margin = 64;
  const int label_y = h - 53;
#else
  const int progress_margin = 12;
  const int progress_y = h - 60;
  const int label_margin = 12;
  const int label_y = h - 33;
#endif

  bar_total = w - 2 * progress_margin - 2 * PROGRESS_MARGIN;
  progress_layer = layer_create(
    GRect(progress_margin, progress_y, w - 2 * progress_margin, 28));
  layer_set_update_proc(progress_layer, progress_paint_callback);
  layer_add_child(root, progress_layer);

  duration_layer = text_layer_create(
    GRect(w / 2, label_y, w / 2 - label_margin, 32));
  text_layer_set_font(duration_layer,
                     fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(duration_layer, GTextAlignmentRight);
  text_layer_set_background_color(duration_layer, GColorClear);
  text_layer_set_text_color(duration_layer, theme_foreground());
  layer_add_child(root, text_layer_get_layer(duration_layer));

  zero_layer = text_layer_create(GRect(label_margin, label_y, 40, 32));
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
  if(load_session()) {
    int restored_total_s = entry->quarts_high * 15;
    int restored_elapsed_s = (int)(time(NULL) - start_s);

    if(restored_total_s <= 0) {
      restored_total_s = 15;
    }
    if(restored_elapsed_s < 0) {
      restored_elapsed_s = 0;
    }
    if(restored_elapsed_s > restored_total_s) {
      restored_elapsed_s = restored_total_s;
    }

    /* Ne rejoue pas une alerte deja passee pendant l'interruption. */
    prev_s = restored_elapsed_s;
  } else {
    start_s = time(NULL);
    save_session();
    prev_s = -1;
  }

  tick_handler(NULL, SECOND_UNIT);
  tick_timer_service_subscribe(SECOND_UNIT, tick_handler);
  steam_start();
}

static void timer_window_disappear(Window *window) {
  steam_stop();
  tick_timer_service_unsubscribe();

  /* La confirmation ne doit pas effacer la session en cours. */
  if(!confirm_open) {
    clear_session();
  }
}

static void timer_window_unload(Window *window) {
  steam_stop();
  tick_timer_service_unsubscribe();
  text_layer_destroy(name_layer);
  text_layer_destroy(clock_layer);
#ifdef PBL_ROUND
  layer_destroy(clock_top_layer);
  clock_top_layer = NULL;
#endif
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
  if(strcmp(tp->value->cstring, "s") == 0) {
    Tuple *sound = dict_find(iter, 9);
    if(sound) {
      sound_enabled = sound->value->int32 != 0;
      persist_write_int(STORE_SOUND, sound_enabled ? 1 : 0);
#ifdef PBL_SPEAKER
      if(!sound_enabled) {
        speaker_stop();
      }
#endif
    }
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
  sound_enabled = persist_exists(STORE_SOUND)
    && persist_read_int(STORE_SOUND) != 0;
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

  confirm_window = window_create();
  window_set_click_config_provider(confirm_window, confirm_click_provider);
  window_set_window_handlers(confirm_window, (WindowHandlers) {
      .load = confirm_window_load,
        .unload = confirm_window_unload,
  });

  window_stack_push(window, true);
}

static void deinit(void) {
  window_stack_pop_all(false);
  window_destroy(window);
  window_destroy(timer_window);
  window_destroy(confirm_window);
  persist_write_int(STORE_CURRENT, selected_entry);
}

int main(void) {
  init();


  app_event_loop();
  deinit();
}
