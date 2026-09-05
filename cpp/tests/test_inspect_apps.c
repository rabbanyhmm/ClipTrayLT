#include <stdio.h>
#include <stdlib.h>
#include <atspi/atspi.h>

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    AtspiAccessible* root = atspi_get_desktop(0);
    if (!root) return 1;

    int count = atspi_accessible_get_child_count(root, NULL);
    for (int i = 0; i < count; ++i) {
        AtspiAccessible* app = atspi_accessible_get_child_at_index(root, i, NULL);
        if (!app) continue;

        int wcount = atspi_accessible_get_child_count(app, NULL);
        for (int j = 0; j < wcount; ++j) {
            AtspiAccessible* win = atspi_accessible_get_child_at_index(app, j, NULL);
            if (!win) continue;
            AtspiStateSet* s = atspi_accessible_get_state_set(win);
            if (s && (atspi_state_set_contains(s, ATSPI_STATE_ACTIVE) || atspi_state_set_contains(s, ATSPI_STATE_FOCUSED))) {
                printf("Active Window found! App: '%s', Win: '%s'\n",
                       atspi_accessible_get_name(app, NULL),
                       atspi_accessible_get_name(win, NULL));
            }
            if (s) g_object_unref(s);
            g_object_unref(win);
        }
        g_object_unref(app);
    }
    g_object_unref(root);
    return 0;
}
