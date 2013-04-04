#include "students_t.h"

#include <boost/math/distributions/students_t.hpp>
using namespace boost::math;

double get_t_value(double alpha, size_t df)
{
    //http://www.boost.org/doc/libs/1_39_0/libs/math/doc/sf_and_dist/html/
    //       math_toolkit/dist/stat_tut/weg/st_eg/tut_mean_intervals.html
    return quantile(complement(students_t(df), alpha / 2));
}
