#include <iostream>
#include <dlfcn.h> 
using namespace std;
typedef int (*funp)();
int main()
{
    int (* testHook)();
    void *lib = dlopen("/sdcard/libgothook.so", RTLD_NOW);
    testHook = (funp) dlsym(lib, "testHook");
    testHook();
}