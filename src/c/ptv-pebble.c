#include <pebble.h>
#include <stdio.h>

#include "message_keys.auto.h"

static Window *s_window;
static TextLayer *s_info_layer;
static MenuLayer *s_menu_layer;

#define MAX_DEPARTURES 20
#define MAX_NAME_LEN 64

typedef struct {
    char name[MAX_NAME_LEN];
    int32_t time;
    int32_t minutes;
} Departure;
static Departure s_departures[MAX_DEPARTURES];
static int s_departure_count = 0;

static int s_scroll_offset = 0;
static int s_label_widths[MAX_DEPARTURES];
static AppTimer *s_scroll_timer = NULL;

static void message_received(DictionaryIterator *iter, void *context) {
    Tuple *tuple = dict_find(iter, MESSAGE_KEY_notConfigured);
    if (tuple) {
        text_layer_set_text(s_info_layer, "Configure the app on your phone.");
        return;
    }

    if (s_departure_count >= MAX_DEPARTURES) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "Maximum number of departures reached.");
        return;
    }

    Departure *departure = &s_departures[s_departure_count++];

    tuple = dict_read_first(iter);
    while (tuple) {
        if (tuple->key == MESSAGE_KEY_name) {
            strcpy(departure->name, tuple->value->cstring);
        } else if (tuple->key == MESSAGE_KEY_time) {
            departure->time = tuple->value->int32;
        } else if (tuple->key == MESSAGE_KEY_minutes) {
            departure->minutes = tuple->value->int32;
        } else {
            APP_LOG(APP_LOG_LEVEL_INFO, "Received key %d", (int)tuple->key);
        }
        tuple = dict_read_next(iter);
    }

    if (s_departure_count == 1) {
        layer_remove_from_parent(text_layer_get_layer(s_info_layer));
        layer_add_child(window_get_root_layer(s_window), menu_layer_get_layer(s_menu_layer));
    }

    menu_layer_reload_data(s_menu_layer);
}

static uint16_t menu_get_num_rows(MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    return s_departure_count;
}

static void menu_draw_row(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
    char buf[16];

    int32_t minutes = s_departures[cell_index->row].minutes;
    snprintf(buf, sizeof(buf), "%ld min", minutes);
    const char *minutes_text = buf;
    if (minutes < 0) {
        minutes_text = "N/A";
    } else if (minutes == 0) {
        minutes_text = "Now";
    }
    graphics_draw_text(
        ctx,
        minutes_text,
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(72, 14, 70, 24),
        GTextOverflowModeFill,
        GTextAlignmentRight,
        NULL
    );

    int32_t time = s_departures[cell_index->row].time;
    snprintf(buf, sizeof(buf), "%ld:%02ld", time / 60, time % 60);
    graphics_draw_text(
        ctx,
        buf,
        fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD),
        GRect(2, 14, 70, 24),
        GTextOverflowModeWordWrap,
        GTextAlignmentLeft,
        NULL
    );

    bool scrolling = s_scroll_offset != 0 && menu_cell_layer_is_highlighted(cell_layer);
    graphics_draw_text(
        ctx,
        s_departures[cell_index->row].name,
        fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
        scrolling ? GRect(2 - s_scroll_offset, 0, 1024, 14) : GRect(2, 0, 140, 14),
        GTextOverflowModeFill,
        GTextAlignmentLeft,
        NULL
    );

    if (s_label_widths[cell_index->row] == 0) {
        GSize size = graphics_text_layout_get_content_size(
            s_departures[cell_index->row].name,
            fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
            GRect(0, 0, 1024, 20),
            GTextOverflowModeFill,
            GTextAlignmentLeft
        );
        s_label_widths[cell_index->row] = size.w;
    }
}

static void cancel_scroll(void *data) {
    s_scroll_offset = 0;
    s_scroll_timer = NULL;
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
}

static void scroll_timer_callback(void *data) {
    size_t row = (size_t)data;
    if (s_scroll_offset + 140 >= s_label_widths[row]) {
        s_scroll_timer = app_timer_register(1000, cancel_scroll, NULL);
        return;
    }
    s_scroll_offset += 4;
    layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
    s_scroll_timer = app_timer_register(100, scroll_timer_callback, (void *)row);
}

static void menu_selection_changed(
    MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context
) {
    s_scroll_offset = 0;
    if (s_scroll_timer) {
        app_timer_cancel(s_scroll_timer);
    }
    s_scroll_timer = app_timer_register(1000, scroll_timer_callback, (void *)(size_t)new_index.row);
}

static void window_load(Window *window) {
    s_info_layer = text_layer_create(GRect(0, 54, 144, 60));
    text_layer_set_font(s_info_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
    text_layer_set_text_alignment(s_info_layer, GTextAlignmentCenter);
    text_layer_set_text(s_info_layer, "Loading...");
    layer_add_child(window_get_root_layer(window), text_layer_get_layer(s_info_layer));

    s_menu_layer = menu_layer_create(GRect(0, 0, 144, 168));
    menu_layer_set_callbacks(
        s_menu_layer,
        NULL,
        (MenuLayerCallbacks){
            .get_num_rows = menu_get_num_rows,
            .draw_row = menu_draw_row,
            .selection_changed = menu_selection_changed,
        }
    );
    menu_layer_set_click_config_onto_window(s_menu_layer, s_window);
    menu_layer_set_highlight_colors(s_menu_layer, GColorRed, GColorWhite);
}

static void window_unload(Window *window) {
    text_layer_destroy(s_info_layer);
    menu_layer_destroy(s_menu_layer);
}

static void init(void) {
    s_window = window_create();
    window_set_window_handlers(
        s_window,
        (WindowHandlers){
            .load = window_load,
            .unload = window_unload,
        }
    );
    window_stack_push(s_window, true);

    app_message_open(128, 0);
    app_message_register_inbox_received(message_received);
}

static void deinit(void) { window_destroy(s_window); }

int main(void) {
    init();
    app_event_loop();
    deinit();
}
