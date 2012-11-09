#include "debug.h"
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include "timeops.h"
#include <pthread.h>
#include "cmm_thread.h"
#include <sstream>
#include <string>
#include <iomanip>
using std::ostringstream; using std::string; 
using std::setw; using std::setfill;

#ifdef ANDROID
#define INSTRUMENTS_LOGFILE "/sdcard/intnw/instruments.log"
#include <android/log.h>
#endif

void get_thread_name(char *name)
{
    snprintf(name, 11, "%08lx", pthread_self());
    return name_str;
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

static bool debugging = true;

void set_debugging(bool debug_on)
{
    debugging = debug_on;
}

void dbgprintf_always(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vdbgprintf(true, fmt, ap);
    va_end(ap);
}

void dbgprintf(const char *fmt, ...)
{
    if (debugging) {
        va_list ap;
        va_start(ap, fmt);
        vdbgprintf(false, fmt, ap);
        va_end(ap);
    }
}
