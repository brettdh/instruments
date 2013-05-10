#define CTEST_MAIN
#include "ctest.h"
#include "debug.h"

int main(int argc, const char *argv[])
{
    set_debug_level(NONE);
    return ctest_main(argc, argv); 
}

