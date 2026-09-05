#include <stdio.h>
#include <stdlib.h>
#include <atspi/atspi.h>

static AtspiAccessible* find_focused(AtspiAccessible* obj) {
    if (!obj) return NULL;

    AtspiStateSet* states = atspi_accessible_get_state_set(obj);
    if (states) {
        gboolean is_focused = atspi_state_set_contains(states, ATSPI_STATE_FOCUSED);
        g_object_unref(states);
        if (is_focused) {
            return obj;
        }
    }

    int count = atspi_accessible_get_child_count(obj, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* child = atspi_accessible_get_child_at_index(obj, i, NULL);
        if (child) {
            AtspiAccessible* res = find_focused(child);
            if (res) {
                if (res != child) g_object_unref(child);
                return res;
            }
            g_object_unref(child);
        }
    }
    return NULL;
}

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    AtspiAccessible* root = atspi_get_desktop(0);
    if (!root) return 1;

    int app_count = atspi_accessible_get_child_count(root, NULL);
    printf("Scanning %d desktop applications for active focus...\n", app_count);

    for (int i = 0; i < app_count; ++i) {
        AtspiAccessible* app = atspi_accessible_get_child_at_index(root, i, NULL);
        if (!app) continue;

        AtspiStateSet* states = atspi_accessible_get_state_set(app);
        gboolean is_active = states && (atspi_state_set_contains(states, ATSPI_STATE_ACTIVE) ||
                                       atspi_state_set_contains(states, ATSPI_STATE_FOCUSED));
        if (states) g_object_unref(states);

        if (is_active) {
            printf("Found active app: %s\n", atspi_accessible_get_name(app, NULL));
            AtspiAccessible* focused = find_focused(app);
            if (focused) {
                printf("  Focused widget: %s, role: %s\n",
                       atspi_accessible_get_name(focused, NULL),
                       atspi_accessible_get_role_name(focused, NULL));

                AtspiText* text = atspi_accessible_get_text_iface(focused);
                if (text) {
                    gint offset = atspi_text_get_caret_offset(text, NULL);
                    printf("  Caret offset: %d\n", offset);
                    AtspiRect* rect = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, NULL);
                    if (rect) {
                        printf("  Caret screen coords: X=%d, Y=%d, W=%d, H=%d\n", rect->x, rect->y, rect->width, rect->height);
                        g_free(rect);
                    }
                    g_object_unref(text);
                }
                if (focused != app) g_object_unref(focused);
            }
        }
        g_object_unref(app);
    }

    return 0;
}
