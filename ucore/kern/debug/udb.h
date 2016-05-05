#ifndef __UDB_H_
#define __UDB_H_

#include <defs.h>

void userDebug(uintptr_t pid, enum DebugSignal sig, uint32_t arg);

#endif __UDB_H_