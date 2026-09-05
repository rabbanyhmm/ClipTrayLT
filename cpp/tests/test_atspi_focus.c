#include <stdio.h>
#include <stdlib.h>
#include <atspi/atspi.h>

static void check_accessible(AtspiAccessible* obj, int depth) {
    if (!obj || depth > 8) return;

    AtspiStateSet* states = atspi_accessible_get_state_set(obj);
    if (states) {
        if (atspi_state_set_contains(states, ATSPI_STATE_FOCUSED)) {
            const char* name = atspi_accessible_get_name(obj, NULL);
            const char* role = atspi_accessible_get_role_name(obj, NULL);
            printf("[FOCUSED] Name: '%s', Role: '%s'\n", name ? name : "", role ? role : "");

            AtspiText* text = atspi_accessible_get_text_iface(obj);
            if (text) {
                gint offset = atspi_text_get_caret_offset(text, NULL);
                printf("  -> Text interface present! Caret offset: %d\n", offset);
                AtspiRect* r = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, NULL);
                if (r) {
                    printf("  -> Caret Rect: X=%d, Y=%d, W=%d, H=%d\n", r->x, r->y, r->width, r->height);
                    g_free(r);
                }
                g_object_unref(text);
            }
        }
        g_object_unref(states);
    }

    int count = atspi_accessible_get_child_count(obj, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* child = atspi_accessible_get_child_at_index(obj, i, NULL);
        if (child) {
            check_accessible(child, depth + 1);
            g_object_unref(child);
        }
    }
}

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    AtspiAccessible* root = atspi_get_desktop(0);
    if (!root) return 1;

    int app_count = atspi_accessible_get_child_count(root, NULL);
    for (int i = 0; i < app_count; ++i) {
        AtspiAccessible* app = atspi_accessible_get_child_at_index(root, i, NULL);
        if (app) {
            check_accessible(app, 0);
            g_object_unref(app);
        }
    }
    return 0;
}
