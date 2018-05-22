#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <dlfcn.h>
#include <sys/mman.h> 
#include "soinfo.h"

using namespace std;

const char *libName = "/sdcard/libgothook.so";
const char *methodName = "puts";


#define MEM_PAGE_SIZE 4096
#define MEM_PAGE_MASK (MEM_PAGE_SIZE-1)
#define MEM_PAGE_START(x)  ((x) & ~MEM_PAGE_MASK)
#define MEM_PAGE_END(x)    MEM_PAGE_START((x) + (MEM_PAGE_SIZE-1))
#define ElfW(type) Elf32_ ## type

const char *getStr(struct soinfo *si, uint32_t r_info)
{
    uint16_t ndx = ELF32_R_SYM(r_info);
    Elf32_Sym *symtab = si->symtab;
    const char *strtab = si->strtab;
    return strtab + symtab[ndx].st_name;
}

int mputs(char *str) 
{
    cout << "---------hook method------------" << endl;
    cout << str << endl;
    return 0;
}

void doGotHook() 
{
    uint32_t gotCount = 0;
    struct soinfo *si = (struct soinfo *) dlopen(libName, RTLD_NOW);
    unsigned base = si->base;
    printf("libstd base address is %x\n", base);
    printf("libstd plt table address is %x\n", si->plt_rel);
    mprotect((void *)si->plt_got, MEM_PAGE_SIZE, PROT_WRITE);
    gotCount = si->plt_rel_count;
    printf("got table count: %x\n", gotCount);
    for (int i = 0; i < gotCount; i++) {
        const char *fn = getStr(si, si->plt_rel[i].r_info);
        if (strcmp(fn, methodName) == 0) {
            void *gotbase = (void *)base + si->plt_rel[i].r_offset;
            printf("find puts method got table: %x\n", gotbase);
            void* pstart = (void*)MEM_PAGE_START(((ElfW(Addr))gotbase));
            mprotect(pstart,MEM_PAGE_SIZE,PROT_READ | PROT_WRITE | PROT_EXEC);
            *(uint32_t *)(gotbase) = (uint32_t)mputs;
            mprotect(pstart,MEM_PAGE_SIZE,PROT_READ | PROT_EXEC);
            printf("my puts address: %x\n", (uint32_t)mputs);
            break;
        }
    }
    }

extern "C" int testHook()
{
    getchar();
    cout << "before hook" << endl;
    puts("hello world");
    doGotHook();
    cout << "after hook" << endl;
    puts("hello world");
}