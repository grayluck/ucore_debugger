
#include <proc.h>
#include <udb.h>
#include <pmm.h>
#include <vmm.h>

/*
struct InstBreakpoint {
    uintptr_t addr;
    uintptr_t vaddr;
    uintptr_t content;
} instBp[MAXBREAKPOINT];
uint32_t instBpN = 0;
*/

struct udbBp_s {
    uintptr_t vaddr;
} udbBp[MAXBREAKPOINT];
uint32_t udbBpN = 0;

int nextStepCount = 0;
intptr_t lastPc;

void udbSleep() {
    bool intr_flag;
    local_intr_save(intr_flag);
    current->state = PROC_SLEEPING;
    current->wait_state = WT_CHILD;
    local_intr_restore(intr_flag);
    schedule();
}
/*
int udbSetInstBp(struct proc_struct* proc, uintptr_t vaddr) {
    uintptr_t la = ROUNDDOWN(vaddr, PGSIZE);
    struct Page * page = get_page(proc->mm->pgdir, la, NULL);
    uint32_t* kaddr = page2kva(page) + (vaddr - la);
    instBp[instBpN].addr = kaddr;
    instBp[instBpN].vaddr = vaddr;
    instBp[instBpN++].content = *kaddr & 0xff;
    *kaddr = (*kaddr & 0xFFFFFF00) | 0xCC;
    return 0;
}

int udbFindInstBp(uintptr_t vaddr) {
    for(int i = 0; i < instBpN; ++i)
        if(instBp[i].addr == vaddr || instBp[i].vaddr == vaddr)
            return i;
    cprintf("[udb error] Instruction breakpoint not found!");
    return -1;
}

int udbUnsetInstBp(int no) {
    uintptr_t* addr = instBp[no].addr;
    *addr = (*addr & 0xFFFFFF00) | instBp[no].content;
    for(int i = no; i < instBpN; ++i)
        instBp[i] = instBp[i+1];
    instBpN --;
}
*/

int udbFindBp(uintptr_t vaddr) {
    for(int i = 0; i < udbBpN; ++i)
        if(udbBp[i].vaddr == vaddr)
            return i;
    return -1;
}

int udbAttach(char* argv[]) {
    int argc = 0;
    while(argv[argc])
        argc++;
    int ret = do_execve(argv[0], argc, argv);
    //udbSetBreakpoint(current, current->tf->tf_eip);
    current->tf->tf_eflags |= 0x100;
    nextStepCount = 1;
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
        return -1;
    wakeup_proc(proc);
    return 0;
}

int udbSetBp(struct proc_struct* proc, uintptr_t vaddr) {
    if(vaddr == 0)
        vaddr = lastPc;
    udbBp[udbBpN++].vaddr = vaddr;
    return vaddr;
}

int udbStepInto(struct proc_struct* proc) {
    nextStepCount = 1;
    udbContinue(proc);
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
        case DEBUG_SETBREAKPOINT:
            return udbSetBp(proc, arg);
        break;
        case DEBUG_CONTINUE:
            return udbContinue(proc);
        break;
        case DEBUG_STEPINTO:
            return udbStepInto(proc);
        break;
    }
}
/*
void udbOnTrap() {
    uintptr_t pc = current->tf->tf_eip;
    udbUnsetInstBp(udbFindBp(pc));
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
    lastPc = pc;
    //cprintf("0x%x\n", pc);
    nextStepCount = nextStepCount-1<0?-1:nextStepCount-1;
    if(nextStepCount == 0 || udbFindBp(pc) >= 0) {
        cprintf("0x%x\n", pc);
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
}
