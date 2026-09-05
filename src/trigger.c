#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;

    const char* xdg = getenv("XDG_RUNTIME_DIR");
    if (xdg && xdg[0] != '\0') {
        snprintf(addr.sun_path, sizeof(addr.sun_path), "%s/cliptraylt_ipc.sock", xdg);
        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
            (void)write(sock, "toggle\n", 7);
            close(sock);
            return 0;
        }
    }

    char tmp_path[108];
    snprintf(tmp_path, sizeof(tmp_path), "/tmp/cliptraylt_ipc_%d.sock", (int)getuid());
    strncpy(addr.sun_path, tmp_path, sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        (void)write(sock, "toggle\n", 7);
        close(sock);
        return 0;
    }

    strncpy(addr.sun_path, "/tmp/cliptraylt_ipc_socket", sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        (void)write(sock, "toggle\n", 7);
        close(sock);
        return 0;
    }

    close(sock);
    return 0;
}
