#include "stopwatch.h"
#include "timeops.h"
#include "debug.h"

#include <string.h>

#include <string>
#include <vector>
#include <sstream>
using std::string; using std::vector; using std::ostringstream;

Stopwatch::Stopwatch()
    : next(-1), last_total(NULL), disabled(false)
{
    last_start.tv_sec = last_start.tv_usec = 0;
}

Stopwatch::Stopwatch(const vector<string>& labels_)
    : labels(labels_), next(-1), last_total(NULL), disabled(false)
{
    last_start.tv_sec = last_start.tv_usec = 0;
    totals.resize(labels.size());
    freezeLabels();
}

void
Stopwatch::setEnabled(bool enabled)
{
    disabled = !enabled;
}


void
Stopwatch::freezeLabels()
{
    next = 0;
}

void 
Stopwatch::start(const char *label)
{
    if (disabled) return;
    
    if (last_total != NULL) {
        stop();
    }

    ASSERT(last_start.tv_sec == 0 &&
           last_start.tv_usec == 0);
    if (next >= 0) {
        ASSERT(labels[next] == label);
        last_total = &totals[next];
    } else {
        labels.push_back(label);
        totals.push_back({0, 0});
        last_total = &totals[totals.size() - 1];
    }
    gettimeofday(&last_start, NULL);
}

void 
Stopwatch::stop()
{
    if (disabled) return;

    ASSERT(last_total != NULL);
    struct timeval now, diff;
    gettimeofday(&now, NULL);
    TIMEDIFF(last_start, now, diff);
    timeradd(last_total, &diff, last_total);
    if (next >= 0) {
        next = (next + 1) % labels.size();
    }
    last_total = NULL;
    memset(&last_start, 0, sizeof(last_start));
}


void 
Stopwatch::reset()
{
    labels.clear();
    totals.clear();
    next = -1;
    last_total = NULL;
    memset(&last_start, 0, sizeof(last_start));
}

string 
Stopwatch::toString()
{
    ostringstream s;
    for (size_t i = 0; i < labels.size(); ++i) {
        s << labels[i] << ": ";
        print_timestamp(s, totals[i]) << "  ";
    }
    return s.str();
}
