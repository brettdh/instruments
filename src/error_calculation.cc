#include "error_calculation.h"

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
