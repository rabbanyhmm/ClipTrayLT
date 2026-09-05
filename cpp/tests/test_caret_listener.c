#include <stdio.h>
#include <stdlib.h>
#include <atspi/atspi.h>
#include <glib.h>

static void on_caret_moved(AtspiEvent* event, void* user_data) {
    (void)user_data;
    if (!event || !event->source) return;

    AtspiText* text = atspi_accessible_get_text_iface(event->source);
    if (!text) return;

    gint offset = atspi_text_get_caret_offset(text, NULL);
    AtspiRect* r = atspi_text_get_character_extents(text, offset > 0 ? offset - 1 : 0, ATSPI_COORD_TYPE_SCREEN, NULL);
    if (r) {
        printf("\n[CARET DETECTED IN REAL-TIME!] App: '%s', Role: '%s', X=%d, Y=%d, W=%d, H=%d (offset=%d)\n",
               atspi_accessible_get_name(atspi_accessible_get_application(event->source, NULL), NULL),
               atspi_accessible_get_role_name(event->source, NULL),
               r->x, r->y, r->width, r->height, offset);
        fflush(stdout);
        g_free(r);
    }
    g_object_unref(text);
}

int main() {
    setenv("AT_SPI_BUS_ADDRESS", "unix:path=/run/user/1000/at-spi/bus", 1);
    atspi_init();

    AtspiEventListener* listener = atspi_event_listener_new(on_caret_moved, NULL, NULL);
    atspi_event_listener_register(listener, "object:text-caret-moved", NULL);
    atspi_event_listener_register(listener, "object:state-changed:focused", NULL);

    printf("Listening for caret movements across all desktop apps (Press Ctrl+C to stop)...\n");
    fflush(stdout);

    GMainLoop* loop = g_main_loop_new(NULL, FALSE);
    g_main_loop_run(loop);

    return 0;
}
