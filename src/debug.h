#ifndef DEBUG_H_INCL_OIUHFBOVSBWE
#define DEBUG_H_INCL_OIUHFBOVSBWE

#include <stdio.h>
#include <unistd.h>

#ifdef __cplusplus
#define CDECL extern "C"
#include <string>
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

#ifdef __cplusplus
namespace instruments {
#endif

enum DebugLevel {
    NONE=0, /* shut it. */
    ERROR,  /* exceptional situations only. */
    INFO,   /* don't put these in hot spots. */
    DEBUG   /* be as verbose and slow as you want. */
};

CDECL void set_debug_level(enum DebugLevel level);
CDECL int is_debugging_on(enum DebugLevel level);

CDECL void dbgprintf(enum DebugLevel level, const char *format, ...)
  __attribute__((format(printf, 2, 3)));
CDECL void dbgprintf_always(const char *format, ...)
  __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
void check(bool success, const std::string& msg);
#endif

#endif
