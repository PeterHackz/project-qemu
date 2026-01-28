#include <QemuDefines.h>

List loaded_dls;

__attribute__((constructor)) void _init_vars() { List_Init(&loaded_dls); }
