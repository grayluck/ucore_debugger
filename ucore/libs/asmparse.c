#include <stdio.h>
#include <string.h>

#define BUFSIZE 1024
static char buf[BUFSIZE];

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

