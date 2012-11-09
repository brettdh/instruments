#ifndef DEBUG_H_INCL_OIUHFBOVSBWE
#define DEBUG_H_INCL_OIUHFBOVSBWE

#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL 
#endif

#define ASSERT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "ASSERT '" #cond "' failed at %s:%d\n", __FILE__, __LINE__); \
            sleep(60);                                                 \
            __builtin_trap();                                          \
        }                                                              \
    } while (0)

CDECL void set_debugging_on(int debug_on);

CDECL void dbgprintf(const char *format, ...)
  __attribute__((format(printf, 1, 2)));
CDECL void dbgprintf_always(const char *format, ...)
  __attribute__((format(printf, 1, 2)));

#endif
