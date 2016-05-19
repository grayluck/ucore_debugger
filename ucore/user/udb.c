#include <stdio.h>
#include <ulib.h>
#include <defs.h>
#include <string.h>
#include <dir.h>
#include <file.h>
#include <error.h>
#include <unistd.h>
#include <stab.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)
#define putc(c)                         printf("%c", c)

#define BUFSIZE                         4096
#define WHITESPACE                      " \t\r\n"
#define SYMBOLS                         "<|>&;"

#define MAXSYMLEN                       64

// definitions for asmparser

#define NO_INFO 0
#define ASM_CODE 1
#define GCC_CODE 2
#define FUNC_DEF 3

struct asminfo {
    char type;
    char buf[256];
    int  pos;
};

int is_hex(const char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

int hex_to_int(const char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    } else if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return 0;
}

/* *
 * asmparse - parse a line in a asm file.
 */
void
asmparse(const char *line, struct asminfo *info) {
    int i, len;
    int flag;
    info->type = 0;
    info->buf[0] = '\0';
    info->pos = 0;
    if (strlen(line) > 10 
            && is_hex(line[0]) 
            && is_hex(line[1])
            && line[9] == '<') {
        info->type = FUNC_DEF;
        for (i = 0; i < 8; i++) {
            info->pos *= 16;
            info->pos += hex_to_int(line[i]);
        }
        for (i = 10; line[i] != '>' && line[i] != '\0'; i++) {
            info->buf[i-10] = line[i];
        }
        info->buf[i-10] = '\0';
        return;
    } else if (strlen(line) > 10 
            && line[0] == ' '
            && line[1] == ' '
            && is_hex(line[2])
            && line[8] == ':') { 
        info->type = ASM_CODE;
        for (i = 2; i < 8; i++) {
            info->pos *= 16;
            info->pos += hex_to_int(line[i]);
        }
        len = strlen(line);
        flag = 0;
        for (i = 2; i < len && flag < 2; i++) {
            if (line[i] == '\t') {
                flag++;
            }
        }
        strcpy(info->buf, line+i);
    } else {
        info->type = GCC_CODE;
        strcpy(info->buf, line);
        return;
    }
}


struct sym_node {
    int pos;
    char *val;
};

char bf[BUFSIZE * 1024];
int bf_n = -1;
// Assembly code
struct sym_node assym[BUFSIZE * 8];
int as_n = 0;
// C code
struct sym_node gcsym[BUFSIZE * 8];
int gc_n = 0;
// Function code
struct sym_node fnsym[BUFSIZE * 8];
int fn_n = 0;

char *
readl_fd(const char *prompt, int fd) {
    static char buffer[BUFSIZE];
    if (prompt != NULL) {
        printf("%s", prompt);
    }
    int ret, i = 0;
    while (1) {
        char c;
        if ((ret = read(fd, &c, sizeof(char))) < 0) {
            return NULL;
        }
        else if (ret == 0) {
            if (i > 0) {
                buffer[i] = '\0';
                break;
            }
            return NULL;
        }

        if (c == 3) {
            return NULL;
        }
        else if (c >= ' ' && i < BUFSIZE - 1) {
            if(fd == 0)
                putc(c);
            buffer[i ++] = c;
        }
        else if (c == '\b' && i > 0) {
            if(fd == 0)
                putc(c);
            i --;
        }
        else if (c == '\n' || c == '\r') {
            if(fd == 0)
                putc(c);
            buffer[i] = '\0';
            break;
        }
    }
    return buffer;
}

char *
readl_raw(const char *prompt) {
    return readl_fd(prompt, 0);
}

char *
readl(int fd) {
    return readl_fd(NULL, fd);
}

char** split(char* s) {
    static char* ret[EXEC_MAX_ARG_NUM + 1];
    int n = strlen(s);
    int cnt = 0;
    for(int i = 0; i < n; ++i) {
        ret[cnt++] = (s + i);
        for(;s[i] != ' ' && i < n; ++i);
        s[i] = 0;
        for(;s[i] == ' ' && i < n; ++i);
    }
    ret[cnt] = 0;
    return ret;
}

uintptr_t strToInt(char* s) {
    return strtol(s, 0, 0);
}

///////////// utilities upside //////////////////////

uint32_t pid;
struct DebugProcessInfo pinfo;

char* subArgv[EXEC_MAX_ARG_NUM + 1];

#define MAXSYM 256
#define MAXBUF 1024

char buf[MAXBUF];
char target[MAXBUF];
char loadedSource[MAXBUF];

char lastInput[MAXBUF];

struct DebugInfo* info = 0;

void uninit() {
    cprintf("child has exited.\n");
    exit(0);
}

uintptr_t getSym(char* s) {
    /*
    for(int i = 0; i < symn; ++i) {
        if(strcmp(sym[i].name, s) == 0)
            return sym[i].addr;
    }*/
    return -1;
}

/*
struct Symbol {
    char name[MAXSYMLEN + 1];
    uint32_t addr;
} sym[MAXSYM];
int symn = 0;

void readSym() {
    symn = 0;
    strcpy(buf, target);
    strcat(buf, ".sym");
    int fil = open(buf, O_RDONLY);
    if(fil < 0)
        cprintf("Failed to load sym file : %s.\n", buf);
    char* tmp;
    while(1) {
        tmp = readl(fil);
        if(tmp == 0)
            break;
        char** s = split(tmp);
        if(strlen(s[1]) > MAXSYMLEN) {
            cprintf("Symbol length too long: %d out of %d\n", strlen(s[1]), MAXSYMLEN);
            s[1][MAXSYMLEN] = 0;
        }
        strcpy(sym[symn].name, s[1]);
        sym[symn].addr = strtol(s[0], 0, 16);
        symn ++;
        if(symn == MAXSYM) {
            cprintf("Symbol list is full.\n");
            break;
        }
    }
    close(fil);
    cprintf("Symbol loaded. %d entries found.\n", symn);
}

uintptr_t getSym(char* s) {
    for(int i = 0; i < symn; ++i) {
        if(strcmp(sym[i].name, s) == 0)
            return sym[i].addr;
    }
    return -1;
}

///////////////// parsing asm file ////////////////
//
void load_asm() {
    as_n = gc_n = fn_n = 0;
    bf_n = -1;
    struct asminfo info;
    strcpy(buf, target);
    strcat(buf, ".asm");
    int fil = open(buf, O_RDONLY);
    int len, i;
    if(fil < 0)
        cprintf("Failed to load asm file : %s.\n", buf);
    char* tmp;
    while(1) {
        tmp = readl(fil);
        if (tmp == 0) {
            break;
        }
        asmparse(tmp, &info);
        if (info.type == FUNC_DEF) {
            fnsym[fn_n].pos = info.pos;
            fnsym[fn_n].val = bf + bf_n + 1;
            strcpy(fnsym[fn_n].val, info.buf);
            bf_n = bf_n + strlen(info.buf) + 1;
            fn_n ++;
        } else if (info.type == ASM_CODE) {
            assym[as_n].pos = info.pos;
            assym[as_n].val = bf + bf_n + 1;
            strcpy(assym[as_n].val, info.buf);
            bf_n = bf_n + strlen(info.buf) + 1;
            // modify pos for c code, so that we know where 
            // in the memory does the c code points to
            for (i = gc_n - 1; i >= 0; i--) {
                if (gcsym[i].pos == 0) {
                    gcsym[i].pos = info.pos;
                } else {
                    break;
                }
            }
            as_n ++;
        } else if (info.type == GCC_CODE) {
            gcsym[gc_n].pos = 0;
            gcsym[gc_n].val = bf + bf_n + 1;
            strcpy(gcsym[gc_n].val, info.buf);
            bf_n = bf_n + strlen(info.buf) + 1;
            gc_n ++;
        }
    }
    close(fil);
}
*/

uint32_t doSysDebug(int sig, int arg) {
    // printf("[debug]%d\n", sig);
    uint32_t ret;
    if((ret = sys_debug(pid, sig, arg)) == -1)
        uninit();
    return ret;
}

void udbWait() {
    doSysDebug(DEBUG_WAIT, &pinfo);
    if(pinfo.state == -1)
        uninit();
    // cprintf("0x%08x\n", pinfo.pc);
}

int udbContinue(int argc, char* argv[]) {
    doSysDebug(DEBUG_CONTINUE, 0);
    udbWait();
    struct DebugInfo* p = findSline(pinfo.pc);
    printCodeLineByDinfo(p);
}

int udbStepInto(int argc, char* argv[]) {
    struct DebugInfo* p = 0;
    while(!(p && 
            p->vaddr == pinfo.pc)) {
        doSysDebug(DEBUG_STEPINTO, 0);
        udbWait();
        p = findSline(pinfo.pc);
    }
    printCodeLineByDinfo(p);
}

int udbInstStepInto(int argc, char* argv[]) {
    doSysDebug(DEBUG_STEPINTO, 0);
    udbWait();
    cprintf("0x%08x\n", pinfo.pc);
    struct DebugInfo* p = findSline(pinfo.pc);
    if(p && p->vaddr == pinfo.pc)
        printCodeLineByDinfo(p);
}

int udbStepOver(int argc, char* argv[]) {
    struct DebugInfo* last = findSline(pinfo.pc);
    if(last == 0) {
        udbInstStepInto(argc, argv);
        return 0;
    }
    if(last->mark == 1) {
        // the last line of function
        udbStepInto(argc, argv);
        return 0;
    }
    struct DebugInfo* p = 0;
    while(!(p && 
            p->vaddr == pinfo.pc && 
            p->func == last->func)) {
        doSysDebug(DEBUG_STEPINTO, 0);
        udbWait();
        p = findSline(pinfo.pc);
    }
    printCodeLineByDinfo(p);
    return 0;
}
/*
int getVaddr(char* s) {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                  
    char* addr = s;
    char* vaddr;
    if(addr[0] == '*') {
        vaddr = strToInt(addr + 1);
    } else {
        vaddr = getSym(addr);
        if(vaddr == -1) {
            cprintf("Cannot find symbol: %s\n", vaddr);
            return -1;
        }
    }
    return vaddr;
}
*/
int udbSetBreakpoint(int argc, char* argv[]) {
    int vaddr;
    char* s = argv[1];
    if(argc == 1) {
        vaddr = 0;
    } else if(s[0] == '*') {
        vaddr = strToInt(s + 1);
    } else {
        struct DebugInfo* p = findFunc(s);
        if(p <= 0) {
            cprintf("Cannot find function: %s\n", s);
            return -1;
        }
        vaddr = p->vaddr;
    }
    uint32_t retAddr = doSysDebug(DEBUG_SETBREAKPOINT, vaddr);
    cprintf("Breakpoint set at 0x%x\n", retAddr);
    return 0;
}

int udbPrint(int argc, char* argv[]) {
    if(argc == 1)
        return;
    int result;
    char* s = argv[1];
    uint32_t vaddr;
    if(s[0] == '$') {
        subArgv[0] = s + 1;
        subArgv[1] = 0;
        result = doSysDebug(DEBUG_PRINT_REG, subArgv);
    } else if(s[0] == '*') {
        vaddr = strToInt(s + 1);
        subArgv[0] = vaddr;
        subArgv[1] = buf;
        subArgv[2] = 0;
        subArgv[3] = 0;
        result = doSysDebug(DEBUG_PRINT, subArgv);
        cprintf("0x%08x : %s\n", vaddr, subArgv[1]);
    } else {
        struct DebugInfo* p = findSymbol(pinfo.pc, s);
        if(p == 0) {
            cprintf("Cannot find symbol: %s\n", s);
            return -1;
        }
        vaddr = p->vaddr;
        int type = 0;
        if(p->type != N_GSYM)
            type = 1;
        subArgv[0] = vaddr;
        subArgv[1] = buf;
        subArgv[2] = type;
        subArgv[3] = 0;
        result = doSysDebug(DEBUG_PRINT, subArgv);
        cprintf("%s : %s\n", s, subArgv[1]);
    }
    return 0;
}

int udbList(int argc, char* argv[]) {
    struct DebugInfo* p = findSline(pinfo.pc);
    if(p == 0) {
        cprintf("No source code to be shown.\n");
        return -1;
    }
    for(int i = p->sourceLine; 
        i < p->sourceLine + 10 && printCodeLine(p->soStr, i) == 0; 
        ++i);
}

int udbQuit(int argc, char* argv[]) {
    cprintf("Quit.\n");
    exit(0);
}

int doHelp(int argc, char* argv[]);

struct command {
    const char *name;
    const char *alias;
    const char *desc;
    int(*func)(int argc, char **argv);
};

static struct command commands[] = {
    {"help", "h", "Display this list of commands.", doHelp},
    {"continue", "c", "Continue running.", udbContinue},
    {"step", "s", "Step into.", udbStepInto},
    {"stepinst", "si", "Step into (instruction).", udbInstStepInto},
    {"next", "n", "Step over.", udbStepOver},
    {"breakpoint", "b", "Set a breakpoint", udbSetBreakpoint},
    {"list", "l", "List source code", udbList},
    {"print", "p", "Print an expression for once", udbPrint},
    {"quit", "q", "Quit udb", udbQuit},
};

#define NCOMMANDS (sizeof(commands)/sizeof(struct command))

int doHelp(int argc, char* argv[]) {
    cprintf("Usage:\n");
    for(int i = 0; i < NCOMMANDS; ++i) {
        cprintf("%8s(%s)    %s\n", commands[i].name, commands[i].alias, commands[i].desc);
    }
}

int main(int argc, char* argv[]) {
    strcpy(target, "test");
    if ((pid = fork()) == 0) {
        subArgv[0] = target;
        subArgv[1] = NULL;
        sys_debug(0, DEBUG_ATTACH, subArgv);
        exit(0);
    }
    info = loadStab(target);
    strcpy(buf, target);
    strcat(buf, ".c");
    strcpy(loadedSource, buf);
    loadCodeFile(buf);
    cprintf("Attached.\n");
    udbWait();
    char* inp_raw;
    while(1) {
        inp_raw = readl_raw("udb>");
        if(inp_raw == NULL)
            break;
        if(strlen(inp_raw) == 0)
            inp_raw = lastInput;
        else
            strcpy(lastInput, inp_raw);
        char** inp = split(inp_raw);
        int cnt = 0;
        while(inp[cnt]) cnt++;
        if(cnt == 0)
            continue;
        for(int i = 0; i < NCOMMANDS; ++i) {
            if(strcmp(commands[i].name, inp[0]) == 0 || strcmp(commands[i].alias, inp[0]) == 0) {
                commands[i].func(cnt, inp);
            }
        }
    }
    return 0;
}
