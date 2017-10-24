/* -*- indent-tabs-mode: nil -*-
 *
 * plthook_elf.c -- implemention of plthook for ELF format
 *
 * URL: https://github.com/kubo/plthook
 *
 * ------------------------------------------------------
 *
 * Copyright 2013-2016 Kubo Takehiro <kubo@jiubao.org>
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
 *       of conditions and the following disclaimer in the documentation and/or other materials
 *       provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ''AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are those of the
 * authors and should not be interpreted as representing official policies, either expressed
 * or implied, of the authors.
 *
 */
#if defined(__sun) && defined(_XOPEN_SOURCE) && !defined(__EXTENSIONS__)
#define __EXTENSIONS__
#endif
#if defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/mman.h>
#include <errno.h>
#include <dlfcn.h>
#ifdef __sun
#include <sys/auxv.h>
#define ELF_TARGET_ALL
#endif /* __sun */
#ifdef __FreeBSD__
#include <sys/types.h>
#include <sys/user.h>
#include <libutil.h>
#endif
#include <elf.h>
#include <link.h>
#include "plthook.h"

#ifndef __GNUC__
#define __attribute__(arg)
#endif

#if defined __FreeBSD__ && defined __i386__ && __ELF_WORD_SIZE == 64
#error 32-bit application on 64-bit OS is not supported.
#endif

#if !defined(R_X86_64_JUMP_SLOT) && defined(R_X86_64_JMP_SLOT)
#define R_X86_64_JUMP_SLOT R_X86_64_JMP_SLOT
#endif

#if defined __x86_64__ || defined __x86_64
#define R_JUMP_SLOT   R_X86_64_JUMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#define PLT_DT_REL    DT_RELA
#define R_GLOBAL_DATA R_X86_64_GLOB_DAT
#elif defined __i386__ || defined __i386
#define R_JUMP_SLOT   R_386_JMP_SLOT
#define Elf_Plt_Rel   Elf_Rel
#define PLT_DT_REL    DT_REL
#define R_GLOBAL_DATA R_386_GLOB_DAT
#elif defined __arm__ || defined __arm
#define R_JUMP_SLOT   R_ARM_JUMP_SLOT
#define Elf_Plt_Rel   Elf_Rel
#elif defined __aarch64__ || defined __aarch64 /* ARM64 */
#define R_JUMP_SLOT   R_AARCH64_JUMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#elif defined __powerpc64__
#define R_JUMP_SLOT   R_PPC64_JMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#elif defined __powerpc__
#define R_JUMP_SLOT   R_PPC_JMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#elif 0 /* disabled because not tested */ && (defined __sparcv9 || defined __sparc_v9__)
#define R_JUMP_SLOT   R_SPARC_JMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#elif 0 /* disabled because not tested */ && (defined __sparc || defined __sparc__)
#define R_JUMP_SLOT   R_SPARC_JMP_SLOT
#define Elf_Plt_Rel   Elf_Rela
#elif 0 /* disabled because not tested */ && (defined __ia64 || defined __ia64__)
#define R_JUMP_SLOT   R_IA64_IPLTMSB
#define Elf_Plt_Rel   Elf_Rela
#else
#error unsupported OS
#endif

#if defined __LP64__
#ifndef ELF_CLASS
#define ELF_CLASS     ELFCLASS64
#endif
#define SIZE_T_FMT "lu"
#define ELF_WORD_FMT "u"
#define ELF_XWORD_FMT "lu"
#define ELF_SXWORD_FMT "ld"
#define Elf_Half Elf64_Half
#define Elf_Xword Elf64_Xword
#define Elf_Sxword Elf64_Sxword
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Sym  Elf64_Sym
#define Elf_Dyn  Elf64_Dyn
#define Elf_Rel  Elf64_Rel
#define Elf_Rela Elf64_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF64_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF64_R_TYPE
#endif
#else /* __LP64__ */
#ifndef ELF_CLASS
#define ELF_CLASS     ELFCLASS32
#endif
#define SIZE_T_FMT "u"
#ifdef __sun
#define ELF_WORD_FMT "lu"
#define ELF_XWORD_FMT "lu"
#define ELF_SXWORD_FMT "ld"
#else
#define ELF_WORD_FMT "u"
#define ELF_XWORD_FMT "u"
#define ELF_SXWORD_FMT "d"
#endif
#define Elf_Half Elf32_Half
#define Elf_Xword Elf32_Word
#define Elf_Sxword Elf32_Sword
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Sym  Elf32_Sym
#define Elf_Dyn  Elf32_Dyn
#define Elf_Rel  Elf32_Rel
#define Elf_Rela Elf32_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF32_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF32_R_TYPE
#endif
#endif /* __LP64__ */

#if defined(PT_GNU_RELRO) && !defined(__sun)
#define SUPPORT_RELRO /* RELRO (RELocation Read-Only) */
#if !defined(DF_1_NOW) && defined(DF_1_BIND_NOW)
#define DF_1_NOW DF_1_BIND_NOW
#endif
#endif

struct plthook {
    const Elf_Sym *dynsym;
    const char *dynstr;
    size_t dynstr_size;
    const char *plt_addr_base;
    const Elf_Plt_Rel *plt;
    size_t plt_cnt;
    Elf_Xword r_type;
#ifdef SUPPORT_RELRO
    const char *relro_start;
    const char *relro_end;
#endif
};

static char errmsg[512];

#ifdef SUPPORT_RELRO
static size_t page_size;
#endif

static int plthook_open_executable(plthook_t **plthook_out);
static int plthook_open_shared_library(plthook_t **plthook_out, const char *filename);
static const Elf_Dyn *find_dyn_by_tag(const Elf_Dyn *dyn, Elf_Sxword tag);
#ifdef SUPPORT_RELRO
static int set_relro_members(plthook_t *plthook, struct link_map *lmap);
#endif
static int plthook_open_real(plthook_t **plthook_out, struct link_map *lmap);
static int check_elf_header(const Elf_Ehdr *ehdr);
static void set_errmsg(const char *fmt, ...) __attribute__((__format__ (__printf__, 1, 2)));

int plthook_open(plthook_t **plthook_out, const char *filename)
{
    *plthook_out = NULL;
    if (filename == NULL) {
        return plthook_open_executable(plthook_out);
    } else {
        return plthook_open_shared_library(plthook_out, filename);
    }
}

int plthook_open_by_handle(plthook_t **plthook_out, void *hndl)
{
    struct link_map *lmap = NULL;

    if (hndl == NULL) {
        set_errmsg("NULL handle");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
        set_errmsg("dlinfo error");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    return plthook_open_real(plthook_out, lmap);
}

int plthook_open_by_address(plthook_t **plthook_out, void *address)
{
#if defined __FreeBSD__
    return PLTHOOK_NOT_IMPLEMENTED;
#else
    Dl_info info;
    struct link_map *lmap = NULL;

    *plthook_out = NULL;
    if (dladdr1(address, &info, (void**)&lmap, RTLD_DL_LINKMAP) == 0) {
        set_errmsg("dladdr error");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    return plthook_open_real(plthook_out, lmap);
#endif
}

static int plthook_open_executable(plthook_t **plthook_out)
{
#if defined __linux__
    return plthook_open_real(plthook_out, _r_debug.r_map);
#elif defined __sun
    const char *auxv_file = "/proc/self/auxv";
#define NUM_AUXV_CNT 10
    FILE *fp = fopen(auxv_file, "r");
    auxv_t auxv;
    struct r_debug *r_debug = NULL;

    if (fp == NULL) {
        set_errmsg("Could not open %s: %s", auxv_file,
                   strerror(errno));
        return PLTHOOK_INTERNAL_ERROR;
    }
    while (fread(&auxv, sizeof(auxv_t), 1, fp) == 1) {
        if (auxv.a_type == AT_SUN_LDDATA) {
            r_debug = (struct r_debug *)auxv.a_un.a_ptr;
            break;
        }
    }
    fclose(fp);
    if (r_debug == NULL) {
        set_errmsg("Could not find r_debug");
        return PLTHOOK_INTERNAL_ERROR;
    }
    return plthook_open_real(plthook_out, r_debug->r_map);
#elif defined __FreeBSD__
    return plthook_open_shared_library(plthook_out, NULL);
#else
    set_errmsg("Opening the main program is not supported on this platform.");
    return PLTHOOK_NOT_IMPLEMENTED;
#endif
}

static int plthook_open_shared_library(plthook_t **plthook_out, const char *filename)
{
    void *hndl = dlopen(filename, RTLD_LAZY | RTLD_NOLOAD);
    struct link_map *lmap = NULL;

    if (hndl == NULL) {
        set_errmsg("dlopen error: %s", dlerror());
        return PLTHOOK_FILE_NOT_FOUND;
    }
    if (dlinfo(hndl, RTLD_DI_LINKMAP, &lmap) != 0) {
        set_errmsg("dlinfo error");
        dlclose(hndl);
        return PLTHOOK_FILE_NOT_FOUND;
    }
    dlclose(hndl);
    return plthook_open_real(plthook_out, lmap);
}

static const Elf_Dyn *find_dyn_by_tag(const Elf_Dyn *dyn, Elf_Sxword tag)
{
    while (dyn->d_tag != DT_NULL) {
        if (dyn->d_tag == tag) {
            return dyn;
        }
        dyn++;
    }
    return NULL;
}

#ifdef SUPPORT_RELRO
#if defined __linux__
static const char *get_mapped_file(const void *address, char *buf, int *err)
{
    unsigned long addr = (unsigned long)address;
    FILE *fp;

    fp = fopen("/proc/self/maps", "r");
    if (fp == NULL) {
        set_errmsg("failed to open /proc/self/maps");
        *err = PLTHOOK_INTERNAL_ERROR;
        return NULL;
    }
    while (fgets(buf, PATH_MAX, fp) != NULL) {
        unsigned long start, end;
        int offset = 0;

        sscanf(buf, "%lx-%lx %*s %*x %*x:%*x %*u %n", &start, &end, &offset);
        if (offset == 0) {
            continue;
        }
        if (start < addr && addr < end) {
            char *p = buf + offset;
            while (*p == ' ') {
                p++;
            }
            if (*p != '/') {
                continue;
            }
            p[strlen(p) - 1] = '\0'; /* remove '\n' */
            fclose(fp);
            return p;
        }
    }
    fclose(fp);
    set_errmsg("Could not find a mapped file reagion containing %p", address);
    *err = PLTHOOK_INTERNAL_ERROR;
    return NULL;
}
#elif defined __FreeBSD__
static const char *get_mapped_file(const void *address, char *buf, int *err)
{
    uint64_t addr = (uint64_t)address;
    struct kinfo_vmentry *top;
    int i, cnt;

    top = kinfo_getvmmap(getpid(), &cnt);
    if (top == NULL) {
        fprintf(stderr, "failed to call kinfo_getvmmap()\n");
        *err = PLTHOOK_INTERNAL_ERROR;
        return NULL;
    }
    for (i = 0; i < cnt; i++) {
        struct kinfo_vmentry *kve = top + i;

        if (kve->kve_start < addr && addr < kve->kve_end) {
            strncpy(buf, kve->kve_path, PATH_MAX);
            free(top);
            return buf;
        }
    }
    free(top);
    set_errmsg("Could not find a mapped file reagion containing %p", address);
    *err = PLTHOOK_INTERNAL_ERROR;
    return NULL;
}
#else
static const char *get_mapped_file(const void *address, char *buf, int *err)
{
    set_errmsg("Could not find a mapped file reagion containing %p", address);
    *err = PLTHOOK_INTERNAL_ERROR;
    return NULL;
}
#endif

static int set_relro_members(plthook_t *plthook, struct link_map *lmap)
{
    char fnamebuf[PATH_MAX];
    const char *fname;
    FILE *fp;
    Elf_Ehdr ehdr;
    Elf_Half idx;
    int rv;

    if (lmap->l_name[0] == '/') {
        fname = lmap->l_name;
    } else {
        int err;

        fname = get_mapped_file(plthook->dynstr, fnamebuf, &err);
        if (fname == NULL) {
            return err;
        }
    }
    fp = fopen(fname, "r");
    if (fp == NULL) {
        set_errmsg("failed to open %s", fname);
        return PLTHOOK_INTERNAL_ERROR;
    }
    if (fread(&ehdr, sizeof(ehdr), 1, fp) != 1) {
        set_errmsg("failed to read the ELF header.");
        fclose(fp);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    rv = check_elf_header(&ehdr);
    if (rv != 0) {
        fclose(fp);
        return rv;
    }

    fseek(fp, ehdr.e_phoff, SEEK_SET);

    for (idx = 0; idx < ehdr.e_phnum; idx++) {
        Elf_Phdr phdr;

        if (fread(&phdr, sizeof(phdr), 1, fp) != 1) {
            set_errmsg("failed to read the program header table.");
            fclose(fp);
            return PLTHOOK_INVALID_FILE_FORMAT;
        }
        if (phdr.p_type == PT_GNU_RELRO) {
            plthook->relro_start = plthook->plt_addr_base + phdr.p_vaddr;
            plthook->relro_end = plthook->relro_start + phdr.p_memsz;
            break;
        }
    }
    fclose(fp);
    return 0;
}
#endif

static int plthook_open_real(plthook_t **plthook_out, struct link_map *lmap)
{
    plthook_t plthook = {NULL,};
    const Elf_Dyn *dyn;
    const char *dyn_addr_base = NULL;

#if defined __linux__
    plthook.plt_addr_base = (char*)lmap->l_addr;
#elif defined __FreeBSD__ || defined __sun
    const Elf_Ehdr *ehdr = (const Elf_Ehdr*)lmap->l_addr;
    int rv = check_elf_header(ehdr);
    if (rv != 0) {
        return rv;
    }
    if (ehdr->e_type == ET_DYN) {
        dyn_addr_base = (const char*)lmap->l_addr;
        plthook.plt_addr_base = (const char*)lmap->l_addr;
    }
#else
#error unsupported OS
#endif

    /* get .dynsym section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_SYMTAB);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_SYMTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynsym = (const Elf_Sym*)(dyn_addr_base + dyn->d_un.d_ptr);

    /* Check sizeof(Elf_Sym) */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_SYMENT);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_SYMTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    if (dyn->d_un.d_val != sizeof(Elf_Sym)) {
        set_errmsg("DT_SYMENT size %" ELF_XWORD_FMT " != %" SIZE_T_FMT, dyn->d_un.d_val, sizeof(Elf_Sym));
        return PLTHOOK_INTERNAL_ERROR;
    }

    /* get .dynstr section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_STRTAB);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_STRTAB");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynstr = dyn_addr_base + dyn->d_un.d_ptr;

    /* get .dynstr size */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_STRSZ);
    if (dyn == NULL) {
        set_errmsg("failed to find DT_STRSZ");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.dynstr_size = dyn->d_un.d_val;

    /* get .rela.plt or .rel.plt section */
    dyn = find_dyn_by_tag(lmap->l_ld, DT_JMPREL);
    plthook.r_type = R_JUMP_SLOT;
#ifdef PLT_DT_REL
    if (dyn == NULL) {
        /* get .rela.dyn or .rel.dyn section */
        dyn = find_dyn_by_tag(lmap->l_ld, PLT_DT_REL);
        plthook.r_type = R_GLOBAL_DATA;
    }
#endif
    if (dyn == NULL) {
        set_errmsg("failed to find DT_JMPREL");
        return PLTHOOK_INTERNAL_ERROR;
    }
    plthook.plt = (const Elf_Plt_Rel *)(dyn_addr_base + dyn->d_un.d_ptr);

    if (plthook.r_type == R_JUMP_SLOT) {
        /* get total size of .rela.plt or .rel.plt */
        dyn = find_dyn_by_tag(lmap->l_ld, DT_PLTRELSZ);
        if (dyn == NULL) {
            set_errmsg("failed to find DT_PLTRELSZ");
            return PLTHOOK_INTERNAL_ERROR;
        }

        plthook.plt_cnt = dyn->d_un.d_val / sizeof(Elf_Plt_Rel);
#ifdef PLT_DT_REL
    } else {
        int total_size_tag = PLT_DT_REL == DT_RELA ? DT_RELASZ : DT_RELSZ;
        int elem_size_tag = PLT_DT_REL == DT_RELA ? DT_RELAENT : DT_RELENT;
        size_t total_size, elem_size;

        dyn = find_dyn_by_tag(lmap->l_ld, total_size_tag);
        if (dyn == NULL) {
            set_errmsg("failed to find 0x%x", total_size_tag);
            return PLTHOOK_INTERNAL_ERROR;
        }
        total_size = dyn->d_un.d_ptr;

        dyn = find_dyn_by_tag(lmap->l_ld, elem_size_tag);
        if (dyn == NULL) {
            set_errmsg("failed to find 0x%x", elem_size_tag);
            return PLTHOOK_INTERNAL_ERROR;
        }
        elem_size = dyn->d_un.d_ptr;
        plthook.plt_cnt = total_size / elem_size;
#endif
    }

#ifdef SUPPORT_RELRO
    dyn = find_dyn_by_tag(lmap->l_ld, DT_FLAGS_1);
    if (dyn != NULL && (dyn->d_un.d_val & DF_1_NOW)) {
        int rv = set_relro_members(&plthook, lmap);
        if (rv != 0) {
            return rv;
        }
        if (page_size == 0) {
            page_size = sysconf(_SC_PAGESIZE);
        }
    }
#endif

    *plthook_out = malloc(sizeof(plthook_t));
    if (*plthook_out == NULL) {
        set_errmsg("failed to allocate memory: %" SIZE_T_FMT " bytes", sizeof(plthook_t));
        return PLTHOOK_OUT_OF_MEMORY;
    }
    **plthook_out = plthook;
    return 0;
}

static int check_elf_header(const Elf_Ehdr *ehdr)
{
    static const unsigned short s = 1;
    /* Check endianness at runtime. */
    unsigned char elfdata = (*(const char*)&s) ? ELFDATA2LSB : ELFDATA2MSB;

    if (ehdr == NULL) {
        set_errmsg("invalid elf header address: NULL");
        return PLTHOOK_INTERNAL_ERROR;
    }

    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        set_errmsg("invalid file signature: 0x%02x,0x%02x,0x%02x,0x%02x",
                   ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_CLASS] != ELF_CLASS) {
        set_errmsg("invalid elf class: 0x%02x", ehdr->e_ident[EI_CLASS]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_DATA] != elfdata) {
        set_errmsg("invalid elf data: 0x%02x", ehdr->e_ident[EI_DATA]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        set_errmsg("invalid elf version: 0x%02x", ehdr->e_ident[EI_VERSION]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        set_errmsg("invalid file type: 0x%04x", ehdr->e_type);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_version != EV_CURRENT) {
        set_errmsg("invalid object file version: %" ELF_WORD_FMT, ehdr->e_version);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ehsize != sizeof(Elf_Ehdr)) {
        set_errmsg("invalid elf header size: %u", ehdr->e_ehsize);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_phentsize != sizeof(Elf_Phdr)) {
        set_errmsg("invalid program header table entry size: %u", ehdr->e_phentsize);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    return 0;
}

int plthook_enum(plthook_t *plthook, unsigned int *pos, const char **name_out, void ***addr_out)
{
    while (*pos < plthook->plt_cnt) {
        const Elf_Plt_Rel *plt = plthook->plt + *pos;
        if (ELF_R_TYPE(plt->r_info) == plthook->r_type) {
            size_t idx = ELF_R_SYM(plt->r_info);

            idx = plthook->dynsym[idx].st_name;
            if (idx + 1 > plthook->dynstr_size) {
                set_errmsg("too big section header string table index: %" SIZE_T_FMT, idx);
                return PLTHOOK_INVALID_FILE_FORMAT;
            }
            *name_out = plthook->dynstr + idx;
            *addr_out = (void**)(plthook->plt_addr_base + plt->r_offset);
            (*pos)++;
            return 0;
        }
        (*pos)++;
    }
    *name_out = NULL;
    *addr_out = NULL;
    return EOF;
}

int plthook_replace(plthook_t *plthook, const char *funcname, void *funcaddr, void **oldfunc)
{
    size_t funcnamelen = strlen(funcname);
    unsigned int pos = 0;
    const char *name;
    void **addr;
    int rv;

    if (plthook == NULL) {
        set_errmsg("invalid argument: The first argument is null.");
        return PLTHOOK_INVALID_ARGUMENT;
    }
    while ((rv = plthook_enum(plthook, &pos, &name, &addr)) == 0) {
        if (strncmp(name, funcname, funcnamelen) == 0) {
            if (name[funcnamelen] == '\0' || name[funcnamelen] == '@') {
#ifdef SUPPORT_RELRO
                void *maddr = NULL;
                if (plthook->relro_start <= (char*)addr && (char*)addr < plthook->relro_end) {
                    maddr = (void*)((size_t)addr & ~(page_size - 1));
                    if (mprotect(maddr, page_size, PROT_READ | PROT_WRITE) != 0) {
                        set_errmsg("Could not change the process memory protection at %p: %s",
                                   maddr, strerror(errno));
                        return PLTHOOK_INTERNAL_ERROR;
                    }
                }
#endif
                if (oldfunc) {
                    *oldfunc = *addr;
                }
                *addr = funcaddr;
#ifdef SUPPORT_RELRO
                if (maddr != NULL) {
                    mprotect(maddr, page_size, PROT_READ);
                }
#endif
                return 0;
            }
        }
    }
    if (rv == EOF) {
        set_errmsg("no such function: %s", funcname);
        rv = PLTHOOK_FUNCTION_NOT_FOUND;
    }
    return rv;
}

void plthook_close(plthook_t *plthook)
{
    if (plthook != NULL) {
        free(plthook);
    }
}

const char *plthook_error(void)
{
    return errmsg;
}

static void set_errmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, ap);
    va_end(ap);
}
