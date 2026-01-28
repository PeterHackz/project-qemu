#include <CryptoUtil.h>
#include <DynamicLinker.h>
#include <Library.h>
#include <Loader.h>

Library libg;

void _nop() {}

int main(void)
{
    Loader_LoadLibrary2("libm.so.6");
    Loader_LoadLibrary2("libstdc++.so.6");

    Loader_LoadLibrary(&libg, "/tmp/libg.so");

    DynamicLinker_RegisterFakeSymbol((FakeSymbol){
        .name = "__cxa_atexit",
        .value = _nop,
    });
    DynamicLinker_ResolveSymbols(&libg);
    DynamicLinker_ExecuteInitializers(&libg);

    CryptoUtil_FindPublicKey(&libg);

    // TODO: free stuff, maybe.

    return 0;
}
