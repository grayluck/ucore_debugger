#include <stdio.h>
#include <ulib.h>
#include <defs.h>

int testValue = 1;

void testSubFunc() {
    int subPartial = 2;
    cprintf("Test sub func.\n");
}

void testFunc() {
    int partial = 1;
    testSubFunc();
    cprintf("Test func.\n");
}

int main(void) {
    cprintf("%d\n", testValue);
    testValue = 2;
    testFunc();
    testValue = 3;
    cprintf("%d\n", testValue);
    return 0;
}
