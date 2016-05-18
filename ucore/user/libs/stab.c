#include <stab.h>
#include <elf.h>
#include <file.h>
#include <defs.h>
#include <unistd.h>
#include <string.h>

#define BUFSIZE                         4096
#define MAXSYMLEN                       10240

off_t nextOffset = 0;

struct Stab {
    uint16_t n_strx;
    uint16_t n_othr;
    uint16_t n_type;
    uint16_t n_desc;
    uint32_t n_value;
} stab[MAXSYMLEN];
int stabn;

struct DebugInfo debugInfo[MAXSYMLEN];

int
load_icode_cont_read(int fd, void *buf, size_t len) {
    return load_icode_read(fd, buf, len, nextOffset);
}

int
load_icode_read(int fd, void *buf, size_t len, off_t offset) {
    int ret;
    nextOffset = offset + len;
    if ((ret = seek(fd, offset, 0)) != 0) {
        return ret;
    }
    if ((ret = read(fd, buf, len)) != len) {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}

char buf[BUFSIZE];

char stabstr[1024 * 1024];   // a stabstr of 1MB
char* stabstrTab[MAXSYMLEN];

char shstrBuf[16 * 1024];   // a stabstr of 16KB
char* shstrtab[100];

char loadedFile[256];
char codeLine[128 * 1024];  // maximum code file limitation: 128KB
char* codeLineTab[16 * 1024];   // cant be more than 16*1024 lines

int findStr(char** arr, char* target) {
    for(int i = 0; arr[i]; ++i) 
    if(strcmp(arr[i], target) == 0)
        return i;
    return -1;
}

void findSection(int fd, struct elfhdr* elf, char* target, struct secthdr* header) {
    struct secthdr __header, *tmpheader = &__header;
    off_t shoff;
    int ret;
    for(int i = 0; i < elf->e_shnum; ++i) {
        shoff = elf->e_shoff + sizeof(struct secthdr) * i;
        if ((ret = load_icode_read(fd, tmpheader, sizeof(struct secthdr), shoff)) != 0) 
            return;
        if(strcmp(shstrBuf + tmpheader->sh_name, target) == 0) {
            memcpy(header, tmpheader, sizeof(struct secthdr));
            return;
        }
    }
}

void outpStabInfo() {
    cprintf("%d %d\n", stabn, sizeof(struct Stab));
    for(int i = 0; i < stabn; ++i) {
        cprintf("%6x %6d %08x %6d %s\n", 
            stab[i].n_type, 
            stab[i].n_desc, stab[i].n_value, stab[i].n_strx,
            stabstr + stab[i].n_strx);
    }
}

void buildDebugInfo() {
    
}

struct DebugInfo* loadStab(char* fil) {
    cprintf("Loading debug information from elf...");
    if(sizeof(enum StabSymbol) == sizeof(char)) {
        cprintf("Compiler not compatible? %d\n", sizeof(enum StabSymbol));
        return -1;
    }
    
    int fd = open(fil, O_RDONLY);
    int ret;
    
    struct elfhdr __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elfhdr), 0)) != 0) 
        return -1;
    
    if (elf->e_magic != ELF_MAGIC) 
        return -1;

    struct secthdr __header, *header = &__header;
    
    off_t shoff, offset;
    
    // first : get the name string of each section
    shoff = elf->e_shoff + sizeof(struct secthdr) * elf->e_shstrndx;
    if ((ret = load_icode_read(fd, header, sizeof(struct secthdr), shoff)) != 0) 
        return -1;
    
    shoff = header->sh_offset;
    offset = 0;
    load_icode_read(fd, shstrBuf, header->sh_size, shoff);
    shstrtab[0] = 0;
    int i = 0, j = 0;
    for(; i < header->sh_size; ++i) {
        if(shstrBuf[i] == 0)
            shstrtab[++j] = shstrBuf + i + 1;
    }
    shstrtab[j] = 0;
    
    // loab stab str
    findSection(fd, elf, ".stabstr", header);
    shoff = header->sh_offset;
    load_icode_read(fd, stabstr, header->sh_size, shoff);
    for(i = 0, j = 0; i < header->sh_size; ++i) {
        if(stabstr[i] == 0)
            stabstrTab[++j] = stabstr + i + 1;
    }
    stabstr[0] = stabstr[j] = 0;
    
    findSection(fd, elf, ".stab", header);
    shoff = header->sh_offset;
    cprintf("%x\n", shoff);
    load_icode_read(fd, stab, header->sh_size, shoff);
    stabn = header->sh_size / sizeof(struct Stab);
    close(fd);
    buildDebugInfo();
    cprintf("Done. %d entries loaded.\n", stabn);
    return debugInfo;
}

void loadCodeFile(char* filename) {
    strcpy(loadedFile, filename);
    int fd = open(filename, O_RDONLY);
    if(fd < 0) {
        cprintf("Load source file %s failed.\n", filename);
        return;
    }
    int ret = read(fd, codeLine, sizeof(codeLine));
    cprintf("%d byte loaded.\n", ret);
    int cnt = 0;
    codeLineTab[0] = codeLine;
    int n = strlen(codeLine);
    for(int i = 0; i < n; ++i) {
        if(codeLine[i] == '\n') {
            codeLineTab[++cnt] = codeLine + i + 1;
            codeLine[i] = 0;
        }
    }
    codeLineTab[cnt] = 0;
    for(int i = 0; i < cnt; ++i)
        cprintf("%s\n", codeLineTab[i]);
    close(fd);
}
