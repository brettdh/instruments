#include "instruments.h"
#include "debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "timeops.h"
#include <pthread.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <stdexcept>
using std::ostringstream; using std::string; 
using std::setw; using std::setfill;
using std::runtime_error;

#ifdef ANDROID
#define INSTRUMENTS_LOGFILE "/sdcard/intnw/instruments.log"
#include <android/log.h>
#endif

static void get_thread_name(char *name)
{
    snprintf(name, 11, "%08lx", pthread_self());
}

static void vdbgprintf(bool plain, const char *fmt, va_list ap)
{
    char thread_name[12];
    get_thread_name(thread_name);

    ostringstream stream;
    if (!plain) {
        struct timeval now;
        TIME(now);
        stream << "[" << now.tv_sec << "." << setw(6) << setfill('0') << now.tv_usec << "]";
        stream << "[" << getpid() << "]";
        stream << "[";
        stream << thread_name;
        stream << "] ";
    }

    string fmtstr(stream.str());
    fmtstr += fmt;
    
#ifdef ANDROID
    FILE *out = fopen(INSTRUMENTS_LOGFILE, "a");
    if (out) {
        vfprintf(out, fmtstr.c_str(), ap);
        fclose(out);
    } else {
        int e = errno;
        stream.str("");
        stream << "Failed opening instruments log file: "
               << strerror(e) << " ** " << fmtstr;
        fmtstr = stream.str();
        
        __android_log_vprint(ANDROID_LOG_INFO, "instruments", fmtstr.c_str(), ap);
    }
#else
    vfprintf(stderr, fmtstr.c_str(), ap);
#endif
}

static instruments::DebugLevel debug_level = instruments::INFO;

void instruments::set_debug_level(enum instruments::DebugLevel level)
{
    debug_level = level;
}

void instruments_set_debug_level(instruments_debug_level_t level_)
{
    int level(level_);
    ASSERT(level >= instruments::NONE && level <= instruments::DEBUG);
    instruments::set_debug_level(instruments::DebugLevel(level));
}

int instruments::is_debugging_on(enum instruments::DebugLevel level)
{
    return (level <= debug_level);
}

void instruments::dbgprintf_always(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdbgprintf(true, fmt, ap);
    va_end(ap);
}

void instruments::dbgprintf(enum instruments::DebugLevel level, const char *fmt, ...)
{
    if (instruments::is_debugging_on(level)) {
        va_list ap;
        va_start(ap, fmt);
        vdbgprintf(false, fmt, ap);
        va_end(ap);
    }
}

#ifdef __cplusplus
void check(bool success, const std::string& msg)
{
    if (!success) {
        throw runtime_error(msg);
    }
}
#endif
