// SPDX-License-Identifier: GPL-3.0-only

#include "config.h"
#include "label.h"
#include "mode.h"
#include "state.h"
#include "utils.h"
#include "utils_cairo.h"

#include <cairo.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

void *tile_mode_enter(struct state *state, struct rect area) {
    struct tile_mode_state *ms = malloc(sizeof(*ms));
    ms->area                   = area;
    ms->label_font_face        = NULL;

    ms->label_symbols =
        label_symbols_from_str(state->config.mode_tile.label_symbols);
    if (ms->label_symbols == NULL) {
        ms->label_selection = NULL;
        state->running      = false;
        return ms;
    }

    int num_symbols = ms->label_symbols->num_symbols;

    ms->sub_area_rows = min(num_symbols, max(ms->area.h, 1));
    ms->sub_area_height_off = ms->area.h % ms->sub_area_rows;
    ms->sub_area_height     = ms->area.h / ms->sub_area_rows;

    ms->sub_area_columns = min(num_symbols, max(ms->area.w, 1));
    ms->sub_area_width_off = ms->area.w % ms->sub_area_columns;
    ms->sub_area_width     = ms->area.w / ms->sub_area_columns;

    ms->label_selection = label_selection_new(
        ms->label_symbols, num_symbols * num_symbols
    );

    ms->label_font_face = cairo_toy_font_face_create(
        state->config.mode_tile.label_font_family, CAIRO_FONT_SLANT_NORMAL,
        CAIRO_FONT_WEIGHT_NORMAL
    );

    return ms;
}

// `tile_mode_back` goes back in history. Returns true if there was something to
// go back to.
static bool tile_mode_back(struct tile_mode_state *mode_state) {
    return label_selection_back(mode_state->label_selection) != 0;
}

// `tile_mode_reenter` reenters the tile mode. We assume that the saved state is
// valid and goes back in history once.
void tile_mode_reenter(struct state *state, void *mode_state) {
    struct tile_mode_state *ms = mode_state;
    tile_mode_back(ms);
}

static struct rect
idx_to_rect(struct tile_mode_state *mode_state, int idx, int x_off, int y_off) {
    int num_symbols = mode_state->label_symbols->num_symbols;
    int column      = idx % num_symbols;
    int row         = idx / num_symbols;

    return (struct rect){
        .x = column * mode_state->sub_area_width +
             min(column, mode_state->sub_area_width_off) + x_off,
        .w = mode_state->sub_area_width +
             (column < mode_state->sub_area_width_off ? 1 : 0),
        .y = row * mode_state->sub_area_height +
             min(row, mode_state->sub_area_height_off) + y_off,
        .h = mode_state->sub_area_height +
             (row < mode_state->sub_area_height_off ? 1 : 0),
    };
}

static bool idx_is_valid(struct tile_mode_state *mode_state, int idx) {
    int num_symbols = mode_state->label_symbols->num_symbols;
    int column      = idx % num_symbols;
    int row         = idx / num_symbols;

    return column < mode_state->sub_area_columns &&
           row < mode_state->sub_area_rows;
}

static bool tile_mode_key(
    struct state *state, void *mode_state, xkb_keysym_t keysym, char *text
) {
    struct tile_mode_state *ms = mode_state;

    switch (keysym) {
    case XKB_KEY_BackSpace:
        return tile_mode_back(ms);
        break;

    case XKB_KEY_Escape:
        state->running = false;
        break;
    default:;
        int symbol_idx = label_symbols_find_idx(ms->label_symbols, text);
        if (symbol_idx < 0) {
            return false;
        }

        if ((ms->label_selection->next == 0 &&
             symbol_idx >= ms->sub_area_columns) ||
            (ms->label_selection->next == 1 &&
             symbol_idx >= ms->sub_area_rows)) {
            return false;
        }

        if (label_selection_append(ms->label_selection, symbol_idx) !=
            LABEL_SELECTION_APPEND_SUCCESS) {
            return false;
        }

        int idx = label_selection_to_idx(ms->label_selection);
        if (idx >= 0) {
            if (!idx_is_valid(ms, idx)) {
                tile_mode_back(ms);
                return false;
            }

            enter_next_mode(
                state, idx_to_rect(ms, idx, ms->area.x, ms->area.y)
            );
        }
        return true;
    }

    return false;
}

void tile_mode_render(struct state *state, void *mode_state, cairo_t *cairo) {
    struct mode_tile_config *config = &state->config.mode_tile;
    struct tile_mode_state  *ms     = mode_state;

    cairo_set_font_face(cairo, ms->label_font_face);
    cairo_set_font_size(
        cairo, compute_relative_font_size(
                   &config->label_font_size, ms->sub_area_height
               )
    );

    cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_paint(cairo);

    cairo_translate(cairo, ms->area.x, ms->area.y);

    cairo_set_source_u32(cairo, config->unselectable_bg_color);
    cairo_rectangle(cairo, .5, .5, ms->area.w - 1, ms->area.h - 1);
    cairo_set_line_width(cairo, 1);
    cairo_stroke(cairo);

    label_selection_t *curr_label = label_selection_new(
        ms->label_symbols,
        ms->label_symbols->num_symbols * ms->label_symbols->num_symbols
    );

    int  label_str_max_len = label_selection_str_max_len(curr_label) + 1;
    char label_selected_str[label_str_max_len];
    char label_unselected_str[label_str_max_len];

    int num_symbols = ms->label_symbols->num_symbols;
    for (int row = 0; row < ms->sub_area_rows; row++) {
        for (int column = 0; column < ms->sub_area_columns; column++) {
            int i = column + row * num_symbols;

            label_selection_set_from_idx(curr_label, i);

            struct rect area = idx_to_rect(ms, i, 0, 0);
            const bool  selectable =
                label_selection_is_included(curr_label, ms->label_selection);

            cairo_set_operator(cairo, CAIRO_OPERATOR_SOURCE);
            if (selectable) {
                cairo_set_source_u32(cairo, config->selectable_bg_color);
                cairo_rectangle(cairo, area.x, area.y, area.w, area.h);
                cairo_fill(cairo);

                cairo_set_source_u32(cairo, config->selectable_border_color);
                cairo_rectangle(
                    cairo, area.x + .5, area.y + .5, area.w - 1, area.h - 1
                );
                cairo_set_line_width(cairo, 1);
                cairo_stroke(cairo);

                cairo_text_extents_t te_all;
                label_selection_str(curr_label, label_selected_str);
                cairo_text_extents(cairo, label_selected_str, &te_all);

                label_selection_str_split(
                    curr_label, label_selected_str, label_unselected_str,
                    ms->label_selection->next
                );

                cairo_text_extents_t te_selected, te_unselected;
                cairo_text_extents(cairo, label_selected_str, &te_selected);
                cairo_text_extents(cairo, label_unselected_str, &te_unselected);

                // Centers the label.
                cairo_move_to(
                    cairo,
                    area.x +
                        (area.w - te_selected.x_advance -
                         te_unselected.x_advance) /
                            2,
                    area.y + (int)((area.h + te_all.height) / 2)
                );
                cairo_set_source_u32(cairo, config->label_select_color);
                cairo_show_text(cairo, label_selected_str);
                cairo_set_source_u32(cairo, config->label_color);
                cairo_show_text(cairo, label_unselected_str);
            }
        }
    }

    label_selection_free(curr_label);
    cairo_translate(cairo, -ms->area.x, -ms->area.y);
}

void tile_mode_state_free(void *mode_state) {
    struct tile_mode_state *ms = mode_state;
    if (ms->label_font_face != NULL) {
        cairo_font_face_destroy(ms->label_font_face);
    }
    label_selection_free(ms->label_selection);
    label_symbols_free(ms->label_symbols);
    free(ms);
}

struct mode_interface tile_mode_interface = {
    .name    = "tile",
    .enter   = tile_mode_enter,
    .reenter = tile_mode_reenter,
    .key     = tile_mode_key,
    .render  = tile_mode_render,
    .free    = tile_mode_state_free,
};
