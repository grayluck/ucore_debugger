#include <stab.h>
#include <elf.h>
#include <file.h>
#include <defs.h>
#include <unistd.h>
#include <string.h>

#define BUFSIZE                         4096
#define MAXSYMLEN                       10240

off_t nextOffset = 0;

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

char symBuf[1024 * 1024];   // a symbuf of 1MB
char* symTab[MAXSYMLEN];

char shstrBuf[16 * 1024];   // a symbuf of 16KB
char* shstrtab[100];

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

int loadElf(char* fil) {
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
    load_icode_read(fd, symBuf, header->sh_size, shoff);
    for(i = 0, j = 0; i < header->sh_size; ++i) {
        if(symBuf[i] == 0)
            symTab[++j] = symBuf + i + 1;
    }
    symBuf[0] = symBuf[j] = 0;
    for(j = 1; symTab[j]; ++j)
        cprintf("%s\n", symTab[j]);
    close(fd);
    return 0;
}
