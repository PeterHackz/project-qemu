#include <Loader.h>
#include <QemuDefines.h>
#define __USE_GNU
#include <Library.h>
#include <List.h>
#include <dlfcn.h>
#include <elf.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int elf_flags_to_prot(const uint32_t flags)
{
    int prot = 0;
    if (flags & PF_R)
        prot |= PROT_READ;
    if (flags & PF_W)
        prot |= PROT_WRITE;
    if (flags & PF_X)
        prot |= PROT_EXEC;
    return prot;
}

void Loader_LoadLibrary(Library *l, const char *lib_path)
{
    Library_init(l);

    const int fd = open(lib_path, O_RDONLY);
    if (fd < 0)
    {
        perror("open");
        return;
    }

    Elf64_Ehdr ehdr;
    if (read(fd, &ehdr, sizeof(ehdr)) != sizeof(ehdr))
    {
        perror("read ehdr");
        close(fd);
        return;
    }

    if (ehdr.e_ident[EI_MAG0] != ELFMAG0 || ehdr.e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr.e_ident[EI_MAG2] != ELFMAG2 || ehdr.e_ident[EI_MAG3] != ELFMAG3)
    {
        fprintf(stderr, "Not a valid ELF file.\n");
        close(fd);
        return;
    }
    if (ehdr.e_machine != EM_AARCH64)
    {
        fprintf(stderr, "Not an AARCH64 ELF file.\n");
        close(fd);
        return;
    }

    const size_t phdr_table_size = ehdr.e_phnum * ehdr.e_phentsize;
    Elf64_Phdr *phdrs = malloc(phdr_table_size);

    if (pread(fd, phdrs, phdr_table_size, ehdr.e_phoff) !=
        (ssize_t)phdr_table_size)
    {
        perror("pread phdrs");
        free(phdrs);
        close(fd);
        return;
    }

    const long page_size = sysconf(_SC_PAGESIZE);
    if (page_size < 0)
    {
        perror("sysconf");
        free(phdrs);
        close(fd);
        return;
    }

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;
    uint64_t min_seg_vaddr = UINT64_MAX; // first byte of first segment
    uint64_t max_actual_vaddr = 0;

    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdrs[i].p_type == PT_LOAD)
        {
            const uint64_t seg_start = phdrs[i].p_vaddr;
            const uint64_t seg_end = seg_start + phdrs[i].p_memsz;

            const uint64_t seg_page_start = seg_start & ~(page_size - 1);
            const uint64_t seg_page_end =
                (seg_end + page_size - 1) & ~(page_size - 1);

            if (seg_page_start < min_vaddr)
                min_vaddr = seg_page_start;
            if (seg_page_end > max_vaddr)
                max_vaddr = seg_page_end;
            if (seg_start < min_seg_vaddr)
                min_seg_vaddr = seg_start;
            if (seg_end > max_actual_vaddr)
                max_actual_vaddr = seg_end;
        }
    }

    // check if we found any LOAD segments
    if (min_vaddr == UINT64_MAX)
    {
        fprintf(stderr, "No PT_LOAD segments found\n");
        free(phdrs);
        close(fd);
        return;
    }

    const size_t total_size = max_vaddr - min_vaddr;
#if DEBUG
    printf("Reserving memory: %p - %p (size: 0x%lx)\n", (void *)min_vaddr,
           (void *)max_vaddr, total_size);
#endif

    // reserve the entire address space
    // P.S: it should probably be PROT_NONE but if I want to iterate over
    // everything in base when scanning over its size I need have at least
    // PROT_READ.
    void *reserved =
        mmap(NULL, total_size, PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED)
    {
        perror("mmap reserve");
        free(phdrs);
        close(fd);
        return;
    }

#if DEBUG
    printf("Reserved region at %p (size: 0x%lx)\n", reserved, total_size);
#endif

    for (int i = 0; i < ehdr.e_phnum; i++)
    {
        if (phdrs[i].p_type == PT_LOAD)
        {
            const uint64_t seg_start = phdrs[i].p_vaddr;
            const uint64_t seg_end = seg_start + phdrs[i].p_memsz;
            const uint64_t seg_page_start = seg_start & ~(page_size - 1);
            const uint64_t seg_page_end =
                (seg_end + page_size - 1) & ~(page_size - 1);
            const size_t seg_map_size = seg_page_end - seg_page_start;

            const uint64_t file_offset =
                phdrs[i].p_offset - (seg_start - seg_page_start);

            void *map_addr = (char *)reserved + (seg_page_start - min_vaddr);
            int prot = elf_flags_to_prot(phdrs[i].p_flags);

#if DEBUG
            printf("Mapping segment %d at %p (size: 0x%lx, file offset: 0x%lx, "
                   "prot: "
                   "%c%c%c)\n",
                   i, map_addr, seg_map_size, file_offset,
                   (prot & PROT_READ) ? 'r' : '-',
                   (prot & PROT_WRITE) ? 'w' : '-',
                   (prot & PROT_EXEC) ? 'x' : '-');
#endif

            const void *mapped = mmap(map_addr, seg_map_size, prot,
                                      MAP_FIXED | MAP_PRIVATE, fd, file_offset);
            if (mapped == MAP_FAILED)
            {
                perror("mmap segment");
                munmap(reserved, total_size);
                free(phdrs);
                close(fd);
                return;
            }
        }
    }

    free(phdrs);
    close(fd);

    void *base_address = (char *)reserved + (min_seg_vaddr - min_vaddr);
    l->base = (uintptr_t)base_address;
    l->size = max_actual_vaddr - min_seg_vaddr;

    printf("[*] QemuLoader: loaded %s at %p-%p (%lu bytes)\n", lib_path,
           (void *)l->base, (void *)(l->base + l->size), l->size);
}

void Loader_LoadLibrary2(const char *lib_path)
{
    void *dl = dlopen(lib_path, RTLD_NOW | RTLD_GLOBAL);
    // global so we can resolve the symbols later from them with dlsym.
    if (dl == NULL)
    {
        printf("[*] QemuLoader_LoadLibrary2: failed to load %s: %s\n", lib_path,
               dlerror());
        return;
    }
    printf("[*] QemuLoader: loaded %s with dlopen\n", lib_path);
    List_Add(&loaded_dls, dl);
}
