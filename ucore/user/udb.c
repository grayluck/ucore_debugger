#include <stdio.h>
#include <ulib.h>
#include <defs.h>
#include <string.h>
#include <dir.h>
#include <file.h>
#include <error.h>
#include <unistd.h>

#define printf(...)                     fprintf(1, __VA_ARGS__)
#define putc(c)                         printf("%c", c)

#define BUFSIZE                         4096
#define WHITESPACE                      " \t\r\n"
#define SYMBOLS                         "<|>&;"

#define MAXSYMLEN                       64

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

char* subArgv[EXEC_MAX_ARG_NUM + 1];

#define MAXSYM 256
#define MAXBUF 1024

struct Symbol {
    char name[MAXSYMLEN + 1];
    uint32_t addr;
} sym[MAXSYM];
int symn = 0;

char buf[MAXBUF];
char target[MAXBUF];

void uninit() {
    cprintf("child has exited.\n");
    exit(0);
}

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

uint32_t doSysDebug(int sig, int arg) {
    uint32_t ret;
    if((ret = sys_debug(pid, sig, arg)) == -1)
        uninit();
    return ret;
}

void udbWait() {
    doSysDebug(DEBUG_WAIT, 0);
}

int udbContinue(int argc, char* argv[]) {
    doSysDebug(DEBUG_CONTINUE, 0);
}

int udbStepInto(int argc, char* argv[]) {
    doSysDebug(DEBUG_STEPINTO, 0);
}

uint32_t getVaddr(char* s) {
    char* addr = s;
    char* vaddr;
    if(addr[0] == '*') {
        vaddr = strToInt(addr + 1);
    } else {
        vaddr = getSym(addr);
        if(vaddr == -1) {
            cprintf("Cannot find symbol: %s\n", addr);
            return -1;
        }
    }
    return vaddr;
}

int udbSetBreakpoint(int argc, char* argv[]) {
    uintptr_t vaddr;
    if(argc == 1) {
        vaddr = 0;
    } else {
        vaddr = getVaddr(argv[1]);
        if(vaddr < 0)
            return -1;
    }
    uint32_t retAddr = doSysDebug(DEBUG_SETBREAKPOINT, vaddr);
    cprintf("Breakpoint set at 0x%x\n", retAddr);
}

int udbPrint(int argc, char* argv[]) {
    if(argc == 1)
        return;
    char* s = argv[1];
    uint32_t vaddr;
    if(s[0] == '$')
        cprintf("reg施工中\n");
    else {
        vaddr = getVaddr(s);
        if(vaddr == -1)
            return -1;
    }
    subArgv[0] = vaddr;
    subArgv[1] = buf;
    int result = doSysDebug(DEBUG_PRINT, subArgv);
    cprintf("0x%x : %s\n", vaddr, subArgv[1]);
}

int udbQuit(int argc, char* argv[]) {
    cprintf("Quit.");
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
    {"breakpoint", "b", "Set a breakpoint", udbSetBreakpoint},
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
    readSym();
    if ((pid = fork()) == 0) {
        subArgv[0] = target;
        subArgv[1] = NULL;
        sys_debug(0, DEBUG_ATTACH, subArgv);
        exit(0);
    }
    cprintf("Attached.\n");
    char* inp_raw;
    while(1) {
        udbWait();
        inp_raw = readl_raw("udb>");
        if(inp_raw == NULL)
            break;
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
