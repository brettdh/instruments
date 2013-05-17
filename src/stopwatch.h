#ifndef STOPWATCH_H_INCL_S0BHU4TU93QG0EW8HUIRS
#define STOPWATCH_H_INCL_S0BHU4TU93QG0EW8HUIRS

#include <string>
#include <vector>
#include <unordered_map>
#include <sys/time.h>

class Stopwatch {
  public:
    Stopwatch();

    // start a new event with the given label.
    // also stop()s the previous event (if any).
    void start(const std::string& label);

    // end the last timing period and add it to its total.
    void stop();

    // reset all timing info.
    void reset();

    // returns a string describing the total times.
    std::string toString();
  private:
    std::vector<std::string> labels_in_order;
    std::unordered_map<std::string, struct timeval> totals;
    decltype(totals.end()) last;
    struct timeval last_start;
};

#endif
