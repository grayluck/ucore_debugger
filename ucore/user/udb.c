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

char *
readline(const char *prompt) {
    static char buffer[BUFSIZE];
    if (prompt != NULL) {
        printf("%s", prompt);
    }
    int ret, i = 0;
    while (1) {
        char c;
        if ((ret = read(0, &c, sizeof(char))) < 0) {
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
            putc(c);
            buffer[i ++] = c;
        }
        else if (c == '\b' && i > 0) {
            putc(c);
            i --;
        }
        else if (c == '\n' || c == '\r') {
            putc(c);
            buffer[i] = '\0';
            break;
        }
    }
    return buffer;
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

#define MAXSYM 200
#define MAXBUF 1024

char* sym[MAXSYM];
int symn = 0;

char buf[MAXBUF];
char target[MAXBUF];

void uninit() {
    cprintf("child has exited.");
    exit(0);
}

void readSym() {
    symn = 0;
    strcpy(buf, target);
    char* tmp = strcat(buf, ".sym");
}

uintptr_t getSym(char* s) {
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

int udbSetBreakpoint(int argc, char* argv[]) {
    uintptr_t vaddr;
    if(argc == 1) {
        vaddr = 0;
    } else {
        char* addr = argv[1];
        if(addr[0] == '*') {
            vaddr = strToInt(addr + 1);
        } else {
            vaddr = getSym(addr);
            if(vaddr == -1) {
                cprintf("Cannot find symbol: %s\n", addr);
                return -1;
            }
        }
    }
    uint32_t retAddr = doSysDebug(DEBUG_SETBREAKPOINT, vaddr);
    cprintf("Breakpoint set at 0x%x\n", retAddr);
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
        inp_raw = readline("udb>");
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
