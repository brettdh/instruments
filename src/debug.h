#ifndef DEBUG_H_INCL_OIUHFBOVSBWE
#define DEBUG_H_INCL_OIUHFBOVSBWE

#define ASSERT(cond)                                                    \
    do {                                                                \
        if (!(cond)) {                                                  \
            fprintf(stderr, "ASSERT '" #cond "' failed at %s:%d\n", __FILE__, __LINE__); \
            sleep(60);                                                 \
            __builtin_trap();                                          \
        }                                                              \
    } while (0)

#endif
