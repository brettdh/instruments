#define CTEST_MAIN
#include "ctest.h"
#include "debug.h"

int main(int argc, const char *argv[])
{
    set_debugging_on(0);
    return ctest_main(argc, argv); 
}

