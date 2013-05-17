#include "stopwatch.h"
#include "timeops.h"

#include <string.h>

#include <string>
#include <vector>
#include <sstream>
using std::string; using std::vector; using std::ostringstream;

Stopwatch::Stopwatch()
{
    last = totals.end();
    cmp_string.reserve(128);
}

void 
Stopwatch::start(const char *label)
{
    if (last != totals.end()) {
        stop();
    }

    cmp_string.assign(label);
    last = totals.find(cmp_string);
    if (last == totals.end()) {
        labels_in_order.push_back(cmp_string);
        totals[cmp_string] = {0, 0};
        last = totals.find(cmp_string);
    }
    gettimeofday(&last_start, NULL);
}

void 
Stopwatch::stop()
{
    if (last != totals.end()) {
        struct timeval now, diff;
        gettimeofday(&now, NULL);
        TIMEDIFF(last_start, now, diff);
        struct timeval& total = last->second;
        timeradd(&total, &diff, &total);
    }
    last = totals.end();
    memset(&last_start, 0, sizeof(last_start));
}

void 
Stopwatch::reset()
{
    labels_in_order.clear();
    totals.clear();
    last = totals.end();
    memset(&last_start, 0, sizeof(last_start));
}

string 
Stopwatch::toString()
{
    ostringstream s;
    for (const string& label : labels_in_order) {
        s << label << ": ";
        print_timestamp(s, totals[label]) << "  ";
    }
    return s.str();
}
