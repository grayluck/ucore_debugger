#include <stdio.h>
#include <ulib.h>
#include <defs.h>

int testValue = 1;

int main(void) {
    cprintf("%d\n", testValue);
    testValue = 2;
    cprintf("%d\n", testValue);
    return 0;
}
