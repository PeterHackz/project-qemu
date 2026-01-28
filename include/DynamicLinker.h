#ifndef QEMU_LINKER_H
#define QEMU_LINKER_H
#include <Library.h>

typedef struct _FakeSymbol
{
    const char *name;
    void *value;
} FakeSymbol;

void DynamicLinker_ResolveSymbols(const Library *l);

void DynamicLinker_ExecuteInitializers(const Library *l);

void DynamicLinker_RegisterFakeSymbol(FakeSymbol symbol);

#endif // QEMU_LINKER_H
