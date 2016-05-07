#include <stdio.h>
#include <ulib.h>
#include <defs.h>

uint32_t pid;

int main(int c, char* s[]) {
    if ((pid = fork()) == 0) {
        sys_debug(0, DEBUG_ATTACH, s[1]);
        exit(0);
    }
    sys_debug(pid, DEBUG_WAIT, 0);
    cprintf("Waited!");
    return 0;
}
