#include <DynamicLinker.h>
#include <Library.h>
#include <QemuDefines.h>
#include <dlfcn.h>
#include <elf.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

List fake_symbols;

static jmp_buf jumper;

void segfault_handler(int sig) { siglongjmp(jumper, 1); }

__attribute__((constructor)) void initFakeSymbols()
{
    List_Init(&fake_symbols);
}

void DynamicLinker_RegisterFakeSymbol(const FakeSymbol symbol)
{
    FakeSymbol *sym = malloc(sizeof(FakeSymbol));
    *sym = symbol;
    List_Add(&fake_symbols, sym);
}

char *replace_substring(const char *str, const char *sub,
                        const char *replacement)
{
    const char *pos = strstr(str, sub);
    if (!pos)
    {
        return strdup(str);
    }

    const size_t len = strlen(str);
    const size_t sub_len = strlen(sub);
    const size_t rep_len = strlen(replacement);

    const size_t new_len = len - sub_len + rep_len;
    char *result = malloc(new_len + 1);
    if (!result)
        return NULL;

    const size_t prefix_len = pos - str;
    memcpy(result, str, prefix_len);

    memcpy(result + prefix_len, replacement, rep_len);

    strcpy(result + prefix_len + rep_len, pos + sub_len);

    return result;
}

int apply_rela(void *base, const char *strtab, const Elf64_Sym *symtab,
               const Elf64_Rela *rel, const size_t count, int *total)
{
    int syms = 0;
    for (size_t i = 0; i < count; i++)
    {
        const Elf64_Rela *r = &rel[i];
        const Elf64_Xword type = ELF64_R_TYPE(r->r_info);
        const Elf64_Xword sym_idx = ELF64_R_SYM(r->r_info);
        void **where = (void **)((char *)base + r->r_offset);

        if (type == R_AARCH64_JUMP_SLOT || type == R_AARCH64_GLOB_DAT)
        {
            (*total)++;
            if (symtab[sym_idx].st_value != 0)
            {
                *where = (void *)(symtab[sym_idx].st_value + (uintptr_t)base);
                continue;
            }
            const char *name = strtab + symtab[sym_idx].st_name;
            *where = NULL;
            for (int j = 0; j < fake_symbols.size; j++)
            {
                const FakeSymbol *fsym =
                    (FakeSymbol *)List_Get(&fake_symbols, j);
                if (strcmp(fsym->name, name) == 0)
                {
                    *where = fsym->value;
                    break;
                }
            }
            if (*where != NULL)
                continue;
            const char *needle = "6__ndk1";
            const char *cxx =
                "7__cxx11"; // since I am using system c++ and not ndk one
            char *fixed_name = replace_substring(name, needle, cxx);
            void *sym = dlsym(RTLD_DEFAULT, fixed_name);
            if (!sym)
            {
                free(fixed_name);
                fixed_name = replace_substring(name, needle, "");
                sym = dlsym(RTLD_DEFAULT, fixed_name);
                if (!sym)
                {
#if DEBUG
                    printf("[*] Unresolved symbol: %s\n", fixed_name);
#endif
                }
                else
                {
                    *where = sym;
#if DEBUG
                    printf("[*] QemuLinker: resolved symbol: %s\n", fixed_name);
#endif
                    syms++;
                }
            }
            else
            {
#if DEBUG
                printf("[*] QemuLinker: resolved symbol: %s\n", fixed_name);
#endif
                *where = sym;
                syms++;
            }
            free(fixed_name);
        }
        else if (type == R_AARCH64_RELATIVE)
        {
            *where = (void *)((char *)base + r->r_addend);
        }
        else if (type == R_AARCH64_ABS64)
        {
            // absolute relocation: store symbol value + addend
            *where = (void *)(symtab[sym_idx].st_value + r->r_addend +
                              (uintptr_t)base);
        }
        else
        {
            fprintf(stderr, "[*] Unsupported reloc type: %ld\n", type);
        }
    }
    return syms;
}

void DynamicLinker_ResolveSymbols(const Library *l)
{
    const Elf64_Ehdr *ehdr = (Elf64_Ehdr *)l->base;
    const Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)l->base + ehdr->e_phoff);

    Elf64_Dyn *dynamic = NULL;

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdrs[i].p_type == PT_DYNAMIC)
        {
            dynamic = (Elf64_Dyn *)((char *)l->base + phdrs[i].p_vaddr);
            break;
        }
    }

    if (!dynamic)
    {
        fprintf(stderr, "No PT_DYNAMIC found\n");
        return;
    }

    Elf64_Sym *symtab = NULL;
    const char *strtab = NULL;
    const Elf64_Rela *rela = NULL;
    Elf64_Xword relasz = 0;
    Elf64_Xword relaent = sizeof(Elf64_Rela);

    const Elf64_Rela *jmprel = NULL;
    Elf64_Xword pltrelsz = 0;

    for (const Elf64_Dyn *dyn = dynamic; dyn->d_tag != DT_NULL; dyn++)
    {
        switch (dyn->d_tag)
        {
        case DT_SYMTAB:
            symtab = (Elf64_Sym *)((char *)l->base + dyn->d_un.d_ptr);
            break;
        case DT_STRTAB:
            strtab = (const char *)((char *)l->base + dyn->d_un.d_ptr);
            break;
        case DT_RELA:
            rela = (Elf64_Rela *)((char *)l->base + dyn->d_un.d_ptr);
            break;
        case DT_RELASZ:
            relasz = dyn->d_un.d_val;
            break;
        case DT_RELAENT:
            relaent = dyn->d_un.d_val;
            break;
        case DT_JMPREL:
            jmprel = (Elf64_Rela *)((char *)l->base + dyn->d_un.d_ptr);
            break;
        case DT_PLTRELSZ:
            pltrelsz = dyn->d_un.d_val;
            break;
        default:;
        }
    }

    if (!symtab || !strtab)
    {
        fprintf(stderr, "Missing symbol or string table\n");
        return;
    }
    int syms = 0;
    int total = 0;
    if (rela && relasz)
    {
        syms += apply_rela((void *)l->base, strtab, symtab, rela,
                           relasz / relaent, &total);
    }

    if (jmprel && pltrelsz)
    {
        syms += apply_rela((void *)l->base, strtab, symtab, jmprel,
                           pltrelsz / sizeof(Elf64_Rela), &total);
    }

    printf("[*] QemuLinker: resolved %d/%d symbols\n", syms, total);
}

void DynamicLinker_ExecuteInitializers(const Library *l)
{
    const Elf64_Ehdr *ehdr = (Elf64_Ehdr *)l->base;
    const Elf64_Phdr *phdrs = (Elf64_Phdr *)((char *)l->base + ehdr->e_phoff);

    Elf64_Dyn *dynamic = NULL;

    for (int i = 0; i < ehdr->e_phnum; i++)
    {
        if (phdrs[i].p_type == PT_DYNAMIC)
        {
            dynamic = (Elf64_Dyn *)((char *)l->base + phdrs[i].p_vaddr);
            break;
        }
    }

    if (!dynamic)
    {
        fprintf(stderr, "No PT_DYNAMIC found\n");
        return;
    }

    void (**init_array)(void) = NULL;
    size_t init_array_size = 0;

    for (Elf64_Dyn *dyn = dynamic; dyn->d_tag != DT_NULL; dyn++)
    {
        if (dyn->d_tag == DT_INIT_ARRAY)
        {
            init_array = (void (**)(void))((char *)l->base + dyn->d_un.d_ptr);
        }
        else if (dyn->d_tag == DT_INIT_ARRAYSZ)
        {
            init_array_size = dyn->d_un.d_val;
        }
    }

    if (init_array && init_array_size)
    {
        struct sigaction sa, old_sa;
        sa.sa_handler = segfault_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGSEGV, &sa, &old_sa);

        const size_t count = init_array_size / sizeof(void *);
#if DEBUG
        printf("[*] QemuLinker: executing %lu constructors\n", count);
#endif
        int execs = 0;
        for (size_t i = 0; i < count; i++)
        {
            void (*init)() = init_array[i];
            // some inits crash (most probably because we "hack" the ndk into
            // this) we do not really need them we care for 1 init that does not
            // crash, so we can just ignore them.
            if (init)
            {
#if DEBUG
                printf("[*] QemuLinker: executing init_array[%lu]: %p\n", i,
                       (void *)((uintptr_t)init - l->base));
#endif
                if (sigsetjmp(jumper, 1) == 0)
                {
                    init();
                    execs++;
                }
                else
                {
#if DEBUG
                    printf("[!] QemuLinker: caught crash in initializer at "
                           "offset %p\n",
                           (void *)((uintptr_t)init - l->base));
#endif
                }
            }
        }
        printf("[*] QemuLinker: executed %d/%lu constructors\n", execs, count);
    }
}
