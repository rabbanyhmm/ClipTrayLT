#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>

int main() {
    int fd4 = open("/dev/input/event4", O_RDONLY | O_NONBLOCK);
    int fd11 = open("/dev/input/event11", O_RDONLY | O_NONBLOCK);
    printf("fd4=%d, fd11=%d\n", fd4, fd11);
    return 0;
}
