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

uint32_t pid;

void uninit() {
    cprintf("child has exited.");
    exit(0);
}

void udbWait() {
    if(sys_debug(pid, DEBUG_WAIT, 0) == -1)
        uninit();
}

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

char* subArgv[EXEC_MAX_ARG_NUM + 1];

#define MAXSYM 200
#define MAXBUF 1024

char* sym[MAXSYM];
int symn = 0;

char buf[MAXBUF];
char target[MAXBUF];

void readSym() {
    symn = 0;
    strcpy(buf, target);
    char* tmp = strcat(buf, ".sym");
}

void getSym(char* s) {
    
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
    while(1) {
        udbWait();
        //readline("udb>");
        if(sys_debug(pid, DEBUG_CONTINUE, 0))
            uninit();
    }
    return 0;
}
