#ifndef QEMU_LIBRARY_H
#define QEMU_LIBRARY_H

#include <stddef.h>
#include <stdint.h>

typedef struct _Library
{
    uintptr_t base;
    size_t size;
} Library;

void Library_init(Library *l);

#define ptr_at(lib, offset) (void *)(lib.base + offset)

#endif // QEMU_LIBRARY_H
