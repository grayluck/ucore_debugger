
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

void udbSetBreakpoint(struct proc_struct* proc, uintptr_t vaddr) {
    uintptr_t la = ROUNDDOWN(vaddr, PGSIZE);
    struct Page * page = get_page(proc->mm->pgdir, la, NULL);
    uint32_t* kaddr = page2kva(page) + (vaddr - la);
    *kaddr = 0x1234567;
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

int udbWait(uint32_t pid) {
    struct proc_struct* childProc = find_proc(pid);
    // TODO may need lock
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

int userDebug(uintptr_t pid, enum DebugSignal sig, uint32_t arg) {
    switch(sig) {
        case DEBUG_ATTACH:
            return udbAttach(arg);
        break;
        case DEBUG_WAIT:
            return udbWait(pid);
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
