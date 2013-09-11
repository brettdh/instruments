#include "error_calculation.h"
#include  <math.h>

double calculate_error(double prev_estimate, double cur_observation)
{
#ifdef RELATIVE_ERROR
    // real estimates should never be exactly zero, 
    //  so here's a hacky workaround for my unit tests
    if (prev_estimate > 0.0) {
        return cur_observation / prev_estimate;
    } else {
        return (cur_observation + 0.001) / (prev_estimate + 0.001);
    }
#else
    return prev_estimate - cur_observation;
#endif
}

double adjusted_estimate(double estimate, double error)
{
#ifdef RELATIVE_ERROR
    return estimate * error;
#else
    return estimate - error;
#endif
}

double no_error_value()
{
#ifdef RELATIVE_ERROR
    return 1.0;
#else
    return 0.0;
#endif
}

double error_midpoint(double lower, double upper)
{
#ifdef RELATIVE_ERROR
    // geometric mean.  for errors symmetric about 1.0, this returns 1.0.
    // for equal errors, this returns the error.
    return sqrt(lower * upper);
#else
    // arithmetic mean.
    return (lower + upper) / 2.0;
#endif
}
