
#include <proc.h>
#include <udb.h>
#include <pmm.h>
#include <vmm.h>

void udbSleep() {
    bool intr_flag;
    local_intr_save(intr_flag);
    current->state = PROC_SLEEPING;
    current->wait_state = WT_CHILD;
    local_intr_restore(intr_flag);
    schedule();
}

uint32_t* oriAddr;
uint32_t oriContent;

int udbSetBreakpoint(struct proc_struct* proc, uintptr_t vaddr) {
    uintptr_t la = ROUNDDOWN(vaddr, PGSIZE);
    struct Page * page = get_page(proc->mm->pgdir, la, NULL);
    uint32_t* kaddr = page2kva(page) + (vaddr - la);
    oriAddr = kaddr;
    oriContent = *kaddr;
    *kaddr |= 0xcc;
    return 0;
}

int udbAttach(const char* name) {
    /*
    const char *name = (const char *)arg[0];
    int argc = (int)arg[1];
    const char **argv = (const char **)arg[2];
    */
    const char *argv[] = {name};
    int ret = do_execve(name, 1, argv);
    udbSetBreakpoint(current, current->tf->tf_eip);
    return 0;
}

int udbWait(struct proc_struct* proc) {
    struct proc_struct* childProc = proc;
    switch(childProc->state) {
    case PROC_SLEEPING:
        // do nothing
        break;
    case PROC_RUNNABLE:
        // sleep current process
        udbSleep();
        break;
    default:
        // child process has exited (unexpected?)
        return -1;
        break;
    }
    return 0;
}

int udbContinue(struct proc_struct* proc) {
    *oriAddr = oriContent;
    wakeup_proc(proc);
}

int userDebug(uintptr_t pid, enum DebugSignal sig, uint32_t arg) {
    struct proc_struct* proc = find_proc(pid);
    switch(sig) {
        case DEBUG_ATTACH:
            return udbAttach(arg);
        break;
        case DEBUG_WAIT:
            return udbWait(proc);
        break;
        case DEBUG_BREAKPOINT:
            return udbSetBreakpoint(proc, arg);
        break;
        case DEBUG_CONTINUE:
            return udbContinue(proc);
        break;
    }
}

void udbOnTrap() {
    // WHAT WE DONT HAVE [REG:PC]!???
    struct proc_struct* parent = current->parent;
    switch(parent->state) {
    case PROC_SLEEPING:
        // wake up parent
        wakeup_proc(parent);
        break;
    case PROC_RUNNABLE:
        // cross fingers and wait parent's turn
        break;
    default:
        // udb doesnt even try to wait
        break;
    }
    udbSleep();
}
