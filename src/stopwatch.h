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
    Stopwatch();
    Stopwatch(const std::vector<std::string>& labels_);

    void setEnabled(bool enabled);

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

    bool disabled;
};

#endif
