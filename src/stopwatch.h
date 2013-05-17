#ifndef STOPWATCH_H_INCL_S0BHU4TU93QG0EW8HUIRS
#define STOPWATCH_H_INCL_S0BHU4TU93QG0EW8HUIRS

#include <string>
#include <vector>
#include <unordered_map>
#include <sys/time.h>
#include <string.h>
#include <assert.h>

#include "timeops.h"

class Stopwatch {
  public:
    Stopwatch(bool disable_=false);

    // start looping over the labels seen so far.
    void freezeLabels();

    // start a new event with the given label.
    // also stop()s the previous event (if any).
    void start(const char *label);

    // end the last timing period and add it to its total.
    void stop();

    // reset all timing info.
    void reset();

    // returns a string describing the total times.
    std::string toString();
  private:
    std::vector<std::string> labels;
    std::vector<struct timeval> totals;
    int next;
    struct timeval *last_total;
    struct timeval last_start;

    bool disable;
};

inline void
Stopwatch::freezeLabels()
{
    next = 0;
}

inline void 
Stopwatch::start(const char *label)
{
    if (disable) return;
    
    if (last_total != NULL) {
        stop();
    }

    assert(last_start.tv_sec == 0 &&
           last_start.tv_usec == 0);
    if (next >= 0) {
        assert(labels[next] == label);
        last_total = &totals[next];
    } else {
        labels.push_back(label);
        totals.push_back({0, 0});
        last_total = &totals[totals.size() - 1];
    }
    gettimeofday(&last_start, NULL);
}

inline void 
Stopwatch::stop()
{
    if (disable) return;

    assert(last_total != NULL);
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

#endif
