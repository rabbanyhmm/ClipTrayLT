#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <atspi/atspi.h>

static AtspiRect* find_focused_text(AtspiAccessible* obj, int depth) {
    if (!obj || depth > 10) return NULL;

    AtspiStateSet* s = atspi_accessible_get_state_set(obj);
    if (!s) return NULL;
    gboolean focused = atspi_state_set_contains(s, ATSPI_STATE_FOCUSED);
    g_object_unref(s);

    if (focused) {
        AtspiText* text = atspi_accessible_get_text_iface(obj);
        if (text) {
            gint offset = atspi_text_get_caret_offset(text, NULL);
            if (offset >= 0) {
                AtspiRect* r = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, NULL);
                g_object_unref(text);
                if (r && (r->x > 0 || r->y > 0)) {
                    return r;
                }
                if (r) g_free(r);
            } else {
                g_object_unref(text);
            }
        }
    }

    int count = atspi_accessible_get_child_count(obj, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* child = atspi_accessible_get_child_at_index(obj, i, NULL);
        if (child) {
            AtspiRect* r = find_focused_text(child, depth + 1);
            g_object_unref(child);
            if (r) return r;
        }
    }
    return NULL;
}

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    AtspiAccessible* root = atspi_get_desktop(0);
    if (!root) return 1;

    AtspiRect* r = NULL;
    int count = atspi_accessible_get_child_count(root, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* app = atspi_accessible_get_child_at_index(root, i, NULL);
        if (!app) continue;

        int wcount = atspi_accessible_get_child_count(app, NULL);
        for (int j = 0; j < wcount; ++j) {
            AtspiAccessible* win = atspi_accessible_get_child_at_index(app, j, NULL);
            if (!win) continue;
            AtspiStateSet* s = atspi_accessible_get_state_set(win);
            gboolean is_active = s && (atspi_state_set_contains(s, ATSPI_STATE_ACTIVE) ||
                                      atspi_state_set_contains(s, ATSPI_STATE_FOCUSED));
            if (s) g_object_unref(s);

            if (is_active) {
                // Search inside this active window
                r = find_focused_text(win, 0);
            }
            g_object_unref(win);
            if (r) break;
        }
        g_object_unref(app);
        if (r) break;
    }
    g_object_unref(root);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    double elapsed_ms = (t1.tv_sec - t0.tv_sec) * 1000.0 + (t1.tv_nsec - t0.tv_nsec) / 1000000.0;
    if (r) {
        printf("Active Window Caret: Found in %.2f ms! X=%d, Y=%d, W=%d, H=%d\n", elapsed_ms, r->x, r->y, r->width, r->height);
        g_free(r);
    } else {
        printf("Active window has no text caret (elapsed %.2f ms)\n", elapsed_ms);
    }
    return 0;
}
