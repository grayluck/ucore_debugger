#ifndef __UDB_H_
#define __UDB_H_

#include <defs.h>

int userDebug(uintptr_t pid, enum DebugSignal sig, uint32_t arg);
void udbOnTrap();

#endif __UDB_H_