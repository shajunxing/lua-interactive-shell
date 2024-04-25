#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void sighandler(int signum) {
    printf("%s,%d %s: %d\n", __FILE__, __LINE__, __func__, signum);
    exit(signum);
}

int main() {
    signal(SIGINT, sighandler);
    for (;;) {
        putchar('.');
        sleep(1);
    }
}