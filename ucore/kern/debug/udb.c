
#include <proc.h>
#include <udb.h>
#include <pmm.h>
#include <vmm.h>

struct breakpoint_s {
    uintptr_t addr;
    uintptr_t vaddr;
    uintptr_t content;
} breakpoints[MAXBREAKPOINT];
uint32_t breakpointN = 0;

void udbSleep() {
    bool intr_flag;
    local_intr_save(intr_flag);
    current->state = PROC_SLEEPING;
    current->wait_state = WT_CHILD;
    local_intr_restore(intr_flag);
    schedule();
}

int udbSetBreakpoint(struct proc_struct* proc, uintptr_t vaddr) {
    uintptr_t la = ROUNDDOWN(vaddr, PGSIZE);
    struct Page * page = get_page(proc->mm->pgdir, la, NULL);
    uint32_t* kaddr = page2kva(page) + (vaddr - la);
    breakpoints[breakpointN].addr = kaddr;
    breakpoints[breakpointN].vaddr = vaddr;
    breakpoints[breakpointN++].content = *kaddr & 0xff;
    //*kaddr = (*kaddr & 0xFFFFFF00) | 0xCC;
    return 0;
}

int udbFindBp(uintptr_t vaddr) {
    for(int i = 0; i < breakpointN; ++i)
        if(breakpoints[i].addr == vaddr || breakpoints[i].vaddr == vaddr)
            return i;
    cprintf("[udb error] breakpoint not found");
    return -1;
}

int udbUnsetBp(int no) {
    uintptr_t* addr = breakpoints[no].addr;
    *addr = (*addr & 0xFFFFFF00) | breakpoints[no].content;
}

int udbAttach(char* argv[]) {
    /*
    const char *name = (const char *)arg[0];
    int argc = (int)arg[1];
    const char **argv = (const char **)arg[2];
    */
    int argc = 0;
    while(argv[argc])
        argc++;
    int ret = do_execve(argv[0], argc, argv);
    udbSetBreakpoint(current, current->tf->tf_eip);
    current->tf->tf_eflags |= 0x100;
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
    if(proc->state == PROC_ZOMBIE)
        return 1;
    wakeup_proc(proc);
    return 0;
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

/*
void udbOnTrap() {
    uintptr_t pc = current->tf->tf_eip;
    udbUnsetBp(udbFindBp(pc));
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
*/

void udbStepTrap() {
    uintptr_t pc = current->tf->tf_eip;
    //cprintf("0x%x\n", pc);
    //udbUnsetBp(udbFindBp(pc));
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
