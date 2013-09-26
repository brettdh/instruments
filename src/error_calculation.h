#ifndef ERROR_CALCULATION_INCL_AHV9AHFOE
#define ERROR_CALCULATION_INCL_AHV9AHFOE

#define RELATIVE_ERROR

double calculate_error(double prev_estimate, double cur_observation);
double adjusted_estimate(double estimate, double error);
double no_error_value();
double error_midpoint(double lower, double upper);

#endif
