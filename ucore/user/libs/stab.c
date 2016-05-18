#include <stab.h>
#include <elf.h>
#include <file.h>
#include <defs.h>
#include <unistd.h>
#include <string.h>

#define BUFSIZE                         4096
#define MAXSYMLEN                       10240

off_t nextOffset = 0;

enum StabSymbol { 
    N_GSYM = 0x20,
    N_FNAME = 0x22,
    N_FUN = 0x24,
    N_STSYM = 0x26,
    N_LCSYM =0x28,
    N_MAIN = 0x2a,
    N_ROSYM = 0x2c,
    N_PC = 0x30,
    N_NSYMS = 0x32,
    N_NOMAP = 0x34,
    N_MAC_DEFINE = 0x36,
    N_OBJ = 0x38,
    N_MAC_UNDEF = 0x3a,
    N_OPT = 0x3c,
    N_RSYM = 0x40,
    N_M2C = 0x42,
    N_SLINE = 0x44,
    N_DSLINE = 0x46,
    N_BSLINE = 0x48,
    N_BROWS = 0x48,
    N_DEFD = 0x4a,
    N_FLINE = 0x4c,
    N_EHDECL = 0x50,
    N_MOD2 = 0x50,
    N_CATCH = 0x54,
    N_SSYM = 0x60,
    N_ENDM = 0x62,
    N_SO = 0x64,
    N_LSYM = 0x80,
    N_BINCL = 0x82,
    N_SOL = 0x84,
    N_PSYM = 0xa0,
    N_EINCL = 0xa2,
    N_ENTRY = 0xa4,
    N_LBRAC = 0xc0,
    N_EXCL = 0xc2,
    N_SCOPE = 0xc4,
    N_RBRAC = 0xe0,
    N_BCOMM = 0xe2,
    N_ECOMM = 0xe4,
    N_ECOML = 0xe8,
    N_WITH = 0xea,
    N_NBTEXT = 0xf0,
    N_NBDATA = 0xf2,
    N_NBBSS = 0xf4,
    N_NBSTS = 0xf6,
    N_NBLCS = 0xf8,
};

struct Stab {
    uint16_t n_strx;
    uint16_t n_othr;
    uint16_t n_type;
    uint16_t n_desc;
    uint32_t n_value;
} stab[MAXSYMLEN];
int stabn;

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
    for(i = 0; i < stabn; ++i) {
        cprintf("%6x %6d %08x %6d %s\n", 
            stab[i].n_type, 
            stab[i].n_desc, stab[i].n_value, stab[i].n_strx,
            stabstr + stab[i].n_strx);
    }
}

int loadElf(char* fil) {
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
    return 0;
}
