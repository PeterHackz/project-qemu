#ifndef QEMU_LOADER_H
#define QEMU_LOADER_H
#include <Library.h>

void Loader_LoadLibrary(Library *l, const char *lib_path);

void Loader_LoadLibrary2(const char *lib_path);

#endif // QEMU_LOADER_H
