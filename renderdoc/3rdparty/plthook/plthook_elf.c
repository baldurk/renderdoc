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
#define _GNU_SOURCE
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include <dlfcn.h>
#ifdef __sun
#include <procfs.h>
#define ELF_TARGET_ALL
#endif /* __sun */
#include <elf.h>
#include <link.h>
#include "plthook.h"

#ifndef __GNUC__
#define __attribute__(arg)
#endif

#if defined __linux__
#define ELF_OSABI     ELFOSABI_SYSV
#define ELF_OSABI_ALT ELFOSABI_LINUX
#elif defined __sun
#define ELF_OSABI     ELFOSABI_SOLARIS
#elif defined __FreeBSD__
#define ELF_OSABI     ELFOSABI_FREEBSD
#if defined __i386__ && __ELF_WORD_SIZE == 64
#error 32-bit application on 64-bit OS is not supported.
#endif
#else
#error unsupported OS
#endif

#if defined __x86_64__ || defined __x86_64
#define ELF_DATA      ELFDATA2LSB
#define E_MACHINE     EM_X86_64
#ifdef R_X86_64_JUMP_SLOT
#define R_JUMP_SLOT   R_X86_64_JUMP_SLOT
#else
#define R_JUMP_SLOT   R_X86_64_JMP_SLOT
#endif
#define SHT_PLT_REL   SHT_RELA
#define Elf_Plt_Rel   Elf_Rela
#define PLT_SECTION_NAME ".rela.plt"
#elif defined __i386__ || defined __i386
#define ELF_DATA      ELFDATA2LSB
#define E_MACHINE     EM_386
#define R_JUMP_SLOT   R_386_JMP_SLOT
#define SHT_PLT_REL   SHT_REL
#define Elf_Plt_Rel   Elf_Rel
#define PLT_SECTION_NAME ".rel.plt"
#else
#error E_MACHINE is not defined.
#endif

#if defined __LP64__
#ifndef ELF_CLASS
#define ELF_CLASS     ELFCLASS64
#endif
#define SIZE_T_FMT "lu"
#define Elf_Half Elf64_Half
#define Elf_Addr Elf64_Addr
#define Elf_Ehdr Elf64_Ehdr
#define Elf_Phdr Elf64_Phdr
#define Elf_Shdr Elf64_Shdr
#define Elf_Sym  Elf64_Sym
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
#define Elf_Half Elf32_Half
#define Elf_Addr Elf32_Addr
#define Elf_Ehdr Elf32_Ehdr
#define Elf_Phdr Elf32_Phdr
#define Elf_Shdr Elf32_Shdr
#define Elf_Sym  Elf32_Sym
#define Elf_Rel  Elf32_Rel
#define Elf_Rela Elf32_Rela
#ifndef ELF_R_SYM
#define ELF_R_SYM ELF32_R_SYM
#endif
#ifndef ELF_R_TYPE
#define ELF_R_TYPE ELF32_R_TYPE
#endif
#endif /* __LP64__ */

struct plthook {
    const char *base;
    const Elf_Phdr *phdr;
    size_t phnum;
    Elf_Shdr *shdr;
    size_t shnum;
    char *shstrtab;
    size_t shstrtab_size;
    const Elf_Sym *dynsym;
    size_t dynsym_cnt;
    const char *dynstr;
    size_t dynstr_size;
    const Elf_Plt_Rel *plt;
    size_t plt_cnt;
#ifdef PT_GNU_RELRO
    const char *relro_start;
    const char *relro_end;
#endif
};

static char errmsg[512];

#ifdef PT_GNU_RELRO
static size_t page_size;
#endif

static int plthook_open_executable(plthook_t **plthook_out);
static int plthook_open_shared_library(plthook_t **plthook_out, const char *filename);
static int plthook_open_real(plthook_t **plthook_out, const char *base, const char *filename);
static int check_elf_header(const Elf_Ehdr *ehdr);
static int find_section(plthook_t *image, const char *name, const Elf_Shdr **out);
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
    return plthook_open_real(plthook_out, (const char*)lmap->l_addr, lmap->l_name);
}

int plthook_open_by_address(plthook_t **plthook_out, void *address)
{
    Dl_info info;

    *plthook_out = NULL;
    if (dladdr(address, &info) == 0) {
        set_errmsg("dladdr error");
        return PLTHOOK_FILE_NOT_FOUND;
    }
    return plthook_open_real(plthook_out, info.dli_fbase, info.dli_fname);
}

static int plthook_open_executable(plthook_t **plthook_out)
{
#if defined __linux__
    /* Open the main program. */
    char buf[128];
    FILE *fp = fopen("/proc/self/maps", "r");
    unsigned long base;

    if (fp == NULL) {
        set_errmsg("Could not open /proc/self/maps: %s",
                   strerror(errno));
        return PLTHOOK_INTERNAL_ERROR;
    }
    if (fgets(buf, sizeof(buf), fp) == NULL) {
        set_errmsg("Could not read /proc/self/maps: %s",
                   strerror(errno));
        fclose(fp);
        return PLTHOOK_INTERNAL_ERROR;
    }
    fclose(fp);
    if (sscanf(buf, "%lx-%*x r-xp %*x %*x:%*x %*u ", &base) != 1) {
        set_errmsg("invalid /proc/self/maps format: %s", buf);
        return PLTHOOK_INTERNAL_ERROR;
    }
    return plthook_open_real(plthook_out, (const char*)base, "/proc/self/exe");
#elif defined __sun
    prmap_t prmap;
    pid_t pid = getpid();
    char fname[128];
    int fd;

    sprintf(fname, "/proc/%d/map", pid);
    fd = open(fname, O_RDONLY);
    if (fd == -1) {
        set_errmsg("Could not open %s: %s", fname,
                   strerror(errno));
        return PLTHOOK_INTERNAL_ERROR;
    }
    if (read(fd, &prmap, sizeof(prmap)) != sizeof(prmap)) {
        set_errmsg("Could not read %s: %s", fname,
                   strerror(errno));
        close(fd);
        return PLTHOOK_INTERNAL_ERROR;
    }
    close(fd);
    sprintf(fname, "/proc/%d/object/a.out", pid);
    return plthook_open_real(plthook_out, (const char*)prmap.pr_vaddr, fname);
#elif defined __FreeBSD__
    return plthook_open_shared_library(plthook_out, NULL);
#else
#error unsupported OS
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
    return plthook_open_real(plthook_out, (const char*)lmap->l_addr, lmap->l_name);
}

static int plthook_open_real(plthook_t **plthook_out, const char *base, const char *filename)
{
    const Elf_Ehdr *ehdr = (Elf_Ehdr *)base;
    const Elf_Shdr *shdr;
    size_t shdr_size;
    int fd = -1;
    off_t offset;
    plthook_t *plthook;
    int rv;
#ifdef PT_GNU_RELRO
    size_t idx;
#endif

    if (base == NULL) {
        set_errmsg("The base address is zero.");
        return PLTHOOK_FILE_NOT_FOUND;
    }

    if (filename == NULL) {
        set_errmsg("failed to get the file name on the disk.");
        return PLTHOOK_FILE_NOT_FOUND;
    }

    plthook = calloc(1, sizeof(plthook_t));
    if (plthook == NULL) {
        set_errmsg("failed to allocate memory: %" SIZE_T_FMT " bytes", sizeof(plthook_t));
        return PLTHOOK_OUT_OF_MEMORY;
    }

    /* sanity check */
    rv = check_elf_header(ehdr);
    if (rv != 0) {
        goto error_exit;
    }
    if (ehdr->e_type == ET_DYN) {
        plthook->base = base;
    }
    plthook->phdr = (const Elf_Phdr *)(plthook->base + ehdr->e_phoff);
    plthook->phnum = ehdr->e_phnum;
    fd = open(filename, O_RDONLY, 0);
    if (fd == -1) {
        set_errmsg("Could not open %s: %s", filename, strerror(errno));
        rv = PLTHOOK_FILE_NOT_FOUND;
        goto error_exit;
    }
    shdr_size = ehdr->e_shnum * ehdr->e_shentsize;
    plthook->shdr = calloc(1, shdr_size);
    if (plthook->shdr == NULL) {
        set_errmsg("failed to allocate memory: %" SIZE_T_FMT " bytes", shdr_size);
        rv = PLTHOOK_OUT_OF_MEMORY;
        goto error_exit;
    }
    offset = ehdr->e_shoff;
    if ((rv = lseek(fd, offset, SEEK_SET)) != offset) {
        set_errmsg("failed to seek to the section header table.");
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    if (read(fd, plthook->shdr, shdr_size) != shdr_size) {
        set_errmsg("failed to read the section header table.");
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    plthook->shnum = ehdr->e_shnum;
    plthook->shstrtab_size = plthook->shdr[ehdr->e_shstrndx].sh_size;
    plthook->shstrtab = malloc(plthook->shstrtab_size);
    if (plthook->shstrtab == NULL) {
        set_errmsg("failed to allocate memory: %" SIZE_T_FMT " bytes", plthook->shstrtab_size);
        rv = PLTHOOK_OUT_OF_MEMORY;
        goto error_exit;
    }
    offset = plthook->shdr[ehdr->e_shstrndx].sh_offset;
    if (lseek(fd, offset, SEEK_SET) != offset) {
        set_errmsg("failed to seek to the section header string table.");
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    if (read(fd, plthook->shstrtab, plthook->shstrtab_size) != plthook->shstrtab_size) {
        set_errmsg("failed to read the section header string table.");
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
#ifdef PT_GNU_RELRO
    if (page_size == 0) {
        page_size = sysconf(_SC_PAGESIZE);
    }
    offset = ehdr->e_phoff;
    if ((rv = lseek(fd, offset, SEEK_SET)) != offset) {
        set_errmsg("failed to seek to the program header table.");
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    for (idx = 0; idx < ehdr->e_phnum; idx++) {
        Elf_Phdr phdr;
        if (read(fd, &phdr, sizeof(phdr)) != sizeof(phdr)) {
            set_errmsg("failed to read the program header table.");
            rv = PLTHOOK_INVALID_FILE_FORMAT;
            goto error_exit;
        }
        if (phdr.p_type == PT_GNU_RELRO) {
            plthook->relro_start = plthook->base + phdr.p_vaddr;
            plthook->relro_end = plthook->relro_start + phdr.p_memsz;
        }
    }
#endif
    close(fd);
    fd = -1;

    rv = find_section(plthook, ".dynsym", &shdr);
    if (rv != 0) {
        goto error_exit;
    }
    if (shdr->sh_type != SHT_DYNSYM) {
        set_errmsg("The type of .dynsym section should be SHT_DYNSYM but %d.", shdr->sh_type);
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    if (shdr->sh_entsize != sizeof(Elf_Sym)) {
        set_errmsg("The size of a section header entry should be sizeof(Elf_Sym)(%" SIZE_T_FMT ") but %" SIZE_T_FMT ".",
                   sizeof(Elf_Sym), shdr->sh_entsize);
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    plthook->dynsym = (const Elf_Sym*)(plthook->base + shdr->sh_addr);
    plthook->dynsym_cnt = shdr->sh_size / shdr->sh_entsize;

    rv = find_section(plthook, ".dynstr", &shdr);
    if (rv != 0) {
        goto error_exit;
    }
    if (shdr->sh_type != SHT_STRTAB) {
        set_errmsg("The type of .dynstrx section should be SHT_STRTAB but %d.", shdr->sh_type);
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    plthook->dynstr = (const char*)(plthook->base + shdr->sh_addr);
    plthook->dynstr_size = shdr->sh_size;

    rv = find_section(plthook, PLT_SECTION_NAME, &shdr);
    if (rv != 0) {
        goto error_exit;
    }
    if (shdr->sh_entsize != sizeof(Elf_Plt_Rel)) {
        set_errmsg("invalid " PLT_SECTION_NAME " table entry size: %" SIZE_T_FMT, shdr->sh_entsize);
        rv = PLTHOOK_INVALID_FILE_FORMAT;
        goto error_exit;
    }
    plthook->plt = (Elf_Plt_Rel *)(plthook->base + shdr->sh_addr);
    plthook->plt_cnt = shdr->sh_size / sizeof(Elf_Plt_Rel);

    *plthook_out = plthook;
    return 0;
 error_exit:
    if (fd != -1) {
        close(fd);
    }
    plthook_close(plthook);
    return rv;
}

int plthook_enum(plthook_t *plthook, unsigned int *pos, const char **name_out, void ***addr_out)
{
    while (*pos < plthook->plt_cnt) {
        const Elf_Plt_Rel *plt = plthook->plt + *pos;
        if (ELF_R_TYPE(plt->r_info) == R_JUMP_SLOT) {
            size_t idx = ELF_R_SYM(plt->r_info);

            if (idx >= plthook->dynsym_cnt) {
                set_errmsg(".dynsym index %" SIZE_T_FMT " should be less than %" SIZE_T_FMT ".", idx, plthook->dynsym_cnt);
                return PLTHOOK_INVALID_FILE_FORMAT;
            }
            idx = plthook->dynsym[idx].st_name;
            if (idx + 1 > plthook->dynstr_size) {
                set_errmsg("too big section header string table index: %" SIZE_T_FMT, idx);
                return PLTHOOK_INVALID_FILE_FORMAT;
            }
            *name_out = plthook->dynstr + idx;
            *addr_out = (void**)(plthook->base + plt->r_offset);
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
#ifdef PT_GNU_RELRO
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
#ifdef PT_GNU_RELRO
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
        free(plthook->shdr);
        free(plthook->shstrtab);
        free(plthook);
    }
}

const char *plthook_error(void)
{
    return errmsg;
}

static int check_elf_header(const Elf_Ehdr *ehdr)
{
    if (memcmp(ehdr->e_ident, ELFMAG, SELFMAG) != 0) {
        set_errmsg("invalid file signature: 0x%02x,0x%02x,0x%02x,0x%02x",
                   ehdr->e_ident[0], ehdr->e_ident[1], ehdr->e_ident[2], ehdr->e_ident[3]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_CLASS] != ELF_CLASS) {
        set_errmsg("invalid elf class: 0x%02x", ehdr->e_ident[EI_CLASS]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_DATA] != ELF_DATA) {
        set_errmsg("invalid elf data: 0x%02x", ehdr->e_ident[EI_DATA]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        set_errmsg("invalid elf version: 0x%02x", ehdr->e_ident[EI_VERSION]);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_ident[EI_OSABI] != ELF_OSABI) {
#ifdef ELF_OSABI_ALT
        if (ehdr->e_ident[EI_OSABI] != ELF_OSABI_ALT) {
            set_errmsg("invalid OS ABI: 0x%02x", ehdr->e_ident[EI_OSABI]);
            return PLTHOOK_INVALID_FILE_FORMAT;
        }
#else
        set_errmsg("invalid OS ABI: 0x%02x", ehdr->e_ident[EI_OSABI]);
        return PLTHOOK_INVALID_FILE_FORMAT;
#endif
    }
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        set_errmsg("invalid file type: 0x%04x", ehdr->e_type);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_machine != E_MACHINE) {
        set_errmsg("invalid machine type: %u", ehdr->e_machine);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    if (ehdr->e_version != EV_CURRENT) {
        set_errmsg("invalid object file version: %u", ehdr->e_version);
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
    if (ehdr->e_shentsize != sizeof(Elf_Shdr)) {
        set_errmsg("invalid section header table entry size: %u", ehdr->e_shentsize);
        return PLTHOOK_INVALID_FILE_FORMAT;
    }
    return 0;
}

static int find_section(plthook_t *image, const char *name, const Elf_Shdr **out)
{
    const Elf_Shdr *shdr = image->shdr;
    const Elf_Shdr *shdr_end = shdr + image->shnum;
    size_t namelen = strlen(name);

    while (shdr < shdr_end) {
        if (shdr->sh_name + namelen >= image->shstrtab_size) {
            set_errmsg("too big section header string table index: %u", shdr->sh_name);
            return PLTHOOK_INVALID_FILE_FORMAT;
        }
        if (strcmp(image->shstrtab + shdr->sh_name, name) == 0) {
            *out = shdr;
            return 0;
        }
        shdr++;
    }
    set_errmsg("failed to find the section header: %s", name);
    return PLTHOOK_INVALID_FILE_FORMAT;
}

static void set_errmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(errmsg, sizeof(errmsg) - 1, fmt, ap);
    va_end(ap);
}
