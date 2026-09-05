#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string.h>

int main() {
    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) return 0;
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, "/tmp/simpleclipboard_ipc_socket", sizeof(addr.sun_path) - 1);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
        (void)write(sock, "show\n", 5);
    }
    close(sock);
    return 0;
}
