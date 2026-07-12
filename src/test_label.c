// SPDX-License-Identifier: GPL-3.0-only

#include "log.h"
#include "src/label.h"

#include <string.h>

int main() {
    label_symbols_t *label_symbols = label_symbols_from_str("abcdé");
    if (!label_symbols) {
        LOG_ERR("`label_symbolss_from_str` should not have returned null.");
        return 1;
    }

    if (label_symbols->num_symbols != 5) {
        LOG_ERR("Expected 5 symbols, got %d.", label_symbols->num_symbols);
        return 2;
    }

    char *s = label_symbols_idx_to_ptr(label_symbols, 0);
    if (strcmp(s, "a")) {
        LOG_ERR("No match");
        LOG_ERR("Given string: '%s'", s);
        return 3;
    }

    char *symbols[] = {
        "a", "b", "c", "d", "é",
    };

    for (int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); i++) {
        int symbol_idx = label_symbols_find_idx(label_symbols, symbols[i]);
        if (symbol_idx != i) {
            LOG_ERR(
                "Wrong index %d (expected %d) for symbol '%s'", symbol_idx, i,
                symbols[i]
            );
            return 4;
        }
    }

    label_selection_t *label_selection =
        label_selection_new(label_symbols, 100);
    int label_selection_str_buf_size =
        label_selection_str_max_len(label_selection) + 1;
    if (label_selection_str_buf_size != 7) {
        LOG_ERR(
            "Wrong label_selection_str_buffer_size = %d",
            label_selection_str_buf_size
        );

        return 5;
    }

    char label_selection_str_buf[label_selection_str_buf_size];
    int  idx;

    for (int i = 0; i < 100; i++) {
        label_selection_set_from_idx(label_selection, i);
        idx = label_selection_to_idx(label_selection);
        if (idx != i) {
            LOG_ERR("Wrong index %d, expected %d.", idx, i);
            label_selection_str(label_selection, label_selection_str_buf);
            LOG_ERR("Selection: '%s'", label_selection_str_buf);
            return 6;
        }
    }

    label_selection_t *exact_power_selection =
        label_selection_new(label_symbols, 25);
    int exact_power_selection_str_buf_size =
        label_selection_str_max_len(exact_power_selection) + 1;
    if (exact_power_selection_str_buf_size != 5) {
        LOG_ERR(
            "Wrong exact power label_selection_str_buffer_size = %d",
            exact_power_selection_str_buf_size
        );
        return 15;
    }
    label_selection_free(exact_power_selection);

    label_selection_clear(label_selection);
    label_selection_append(label_selection, 4);
    label_selection_append(label_selection, 2);

    label_selection_str(label_selection, label_selection_str_buf);
    if (strcmp(label_selection_str_buf, "éc")) {
        LOG_ERR("Wrong selection string '%s'", label_selection_str_buf);
        return 7;
    }

    label_selection_append(label_selection, 0);
    if ((idx = label_selection_to_idx(label_selection)) != 14) {
        LOG_ERR("Wrong index %d", idx);
        return 8;
    }

    if (!label_selection_incr(label_selection)) {
        LOG_ERR("We should not be overflowing here.");
        return 9;
    }

    if ((idx = label_selection_to_idx(label_selection)) != 15) {
        LOG_ERR("Wrong index %d", idx);
        return 10;
    }

    char label_selection_str_buf_1[label_selection_str_buf_size];

    label_selection_str_split(
        label_selection, label_selection_str_buf, label_selection_str_buf_1, 1
    );
    if (strcmp(label_selection_str_buf, "a")) {
        LOG_ERR("Wrong prefix '%s'", label_selection_str_buf);
        return 11;
    }
    if (strcmp(label_selection_str_buf_1, "da")) {
        LOG_ERR("Wrong suffix '%s'", label_selection_str_buf_1);
        return 12;
    }

    // Tests with the unicode character not at end-of-string

    label_symbols_t *alt_label_symbols = label_symbols_from_str("abcdéfghi");
    label_selection_t *alt_selection =
        label_selection_new(alt_label_symbols, 100);
    int alt_selection_str_buf_size =
        label_selection_str_max_len(alt_selection) + 1;
    if (alt_selection_str_buf_size != 7) {
        LOG_ERR(
            "Wrong label_selection_str_buffer_size = %d",
            alt_selection_str_buf_size
        );

        return 13;
    }

    char *alt_symbols[] = {
        "a", "b", "c", "d", "é", "f", "g", "h", "i",
    };

    for (int i = 0; i < 9; i++) {
        int symbol_idx =
            label_symbols_find_idx(alt_label_symbols, alt_symbols[i]);
        if (symbol_idx != i) {
            LOG_ERR(
                "Wrong index %d (expected %d) for symbol '%s'", symbol_idx, i,
                symbols[i]
            );
            return 14;
        }
    }

    label_symbols_t *binary_label_symbols = label_symbols_from_str("ab");
    label_selection_t *binary_selection =
        label_selection_new(binary_label_symbols, 8);
    if (label_selection_str_max_len(binary_selection) + 1 != 4) {
        LOG_ERR("Wrong binary label string buffer size.");
        return 16;
    }
    if (label_selection_append(binary_selection, 1) ||
        label_selection_append(binary_selection, 1) ||
        label_selection_append(binary_selection, 1)) {
        LOG_ERR("Could not append a 3-character binary label.");
        return 17;
    }
    if ((idx = label_selection_to_idx(binary_selection)) != 7) {
        LOG_ERR("Wrong binary label index %d", idx);
        return 18;
    }
    if (label_selection_append(binary_selection, 0) !=
        LABEL_SELECTION_APPEND_FULL) {
        LOG_ERR("Expected binary label selection to be full.");
        return 19;
    }
    label_selection_free(binary_selection);
    label_symbols_free(binary_label_symbols);

    char long_symbols[130];
    memset(long_symbols, 'a', sizeof(long_symbols) - 1);
    long_symbols[sizeof(long_symbols) - 1] = '\0';
    label_symbols_t *long_label_symbols =
        label_symbols_from_str(long_symbols);
    if (long_label_symbols != NULL) {
        LOG_ERR("Expected overly long label symbols to fail.");
        label_symbols_free(long_label_symbols);
        return 20;
    }

    label_selection_free(alt_selection);
    label_symbols_free(alt_label_symbols);
    label_selection_free(label_selection);
    label_symbols_free(label_symbols);
    return 0;
}
