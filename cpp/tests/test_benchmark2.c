#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <atspi/atspi.h>

static AtspiRect* find_caret_recursive(AtspiAccessible* obj, int depth) {
    if (!obj || depth > 8) return NULL;

    AtspiStateSet* states = atspi_accessible_get_state_set(obj);
    if (states) {
        gboolean is_focused = atspi_state_set_contains(states, ATSPI_STATE_FOCUSED);
        g_object_unref(states);
        if (is_focused) {
            AtspiText* text = atspi_accessible_get_text_iface(obj);
            if (text) {
                gint offset = atspi_text_get_caret_offset(text, NULL);
                AtspiRect* r = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, NULL);
                g_object_unref(text);
                if (r && (r->x > 0 || r->y > 0)) {
                    return r;
                }
                if (r) g_free(r);
            }
        }
    }

    int count = atspi_accessible_get_child_count(obj, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* child = atspi_accessible_get_child_at_index(obj, i, NULL);
        if (child) {
            AtspiRect* r = find_caret_recursive(child, depth + 1);
            g_object_unref(child);
            if (r) return r;
        }
    }
    return NULL;
}

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    for (int run = 0; run < 3; ++run) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);

        AtspiAccessible* root = atspi_get_desktop(0);
        AtspiRect* r = NULL;
        int count = atspi_accessible_get_child_count(root, NULL);
        for (int i = 0; i < count; ++i) {
            AtspiAccessible* app = atspi_accessible_get_child_at_index(root, i, NULL);
            if (app) {
                r = find_caret_recursive(app, 0);
                g_object_unref(app);
                if (r) break;
            }
        }
        g_object_unref(root);

        clock_gettime(CLOCK_MONOTONIC, &t1);
        double elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
        if (r) {
            printf("Run %d: Caret found in %.2f ms! X=%d, Y=%d\n", run, elapsed_ms, r->x, r->y);
            g_free(r);
        }
    }
    return 0;
}
