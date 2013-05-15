#ifndef DEBUG_H_INCL_OIUHFBOVSBWE
#define DEBUG_H_INCL_OIUHFBOVSBWE

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

#include <stdio.h>
#include <unistd.h>

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

void set_debug_level(enum DebugLevel level);
int is_debugging_on(enum DebugLevel level);

void dbgprintf(enum DebugLevel level, const char *format, ...)
  __attribute__((format(printf, 2, 3)));
void dbgprintf_always(const char *format, ...)
  __attribute__((format(printf, 1, 2)));

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <string>
void check(bool success, const std::string& msg);
#endif

#endif
