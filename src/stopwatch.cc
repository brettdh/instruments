#include "stopwatch.h"
#include "timeops.h"

#include <string.h>

#include <string>
#include <vector>
#include <sstream>
using std::string; using std::vector; using std::ostringstream;

Stopwatch::Stopwatch(bool disable_)
    : next(-1), last_total(NULL), disable(disable_)
{
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
