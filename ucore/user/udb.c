#include <stdio.h>
#include <ulib.h>
#include <defs.h>

uint32_t pid;

void uninit() {
    exit(0);
}

void udbWait() {
    if(sys_debug(pid, DEBUG_WAIT, 0) == -1) {
        cprintf("child has exited.");
        uninit();
    }
}

int main(int c, char* s[]) {
    if ((pid = fork()) == 0) {
        sys_debug(0, DEBUG_ATTACH, s[1]);
        exit(0);
    }
    udbWait();
    cprintf("Test passed!");
    return 0;
}
