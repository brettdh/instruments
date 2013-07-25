#include "continuous_distribution.h"


// creates a Weibull distribution.
ContinuousDistribution::ContinuousDistribution(double shape_, double scale_)
    : distribution(shape, scale)
{
}

// returns the probability that the value is less than upper,
//   given that it is greater than lower.
double 
ContinuousDistribution::getProbabilityValueIsInRange(double lower, double upper)
{
    /* Pr(X < upper | X >= lower)
     *  = Pr(X < upper ^ X >= lower) / Pr(X >= lower)
     *  = ( F(upper) - F(lower) ) / (1 - F(lower))
     */
    return (cdf(upper) - cdf(lower)) / (1.0 - cdf(lower));
}

inline double
ContinuousDistribution::cdf(double value)
{
    return boost::math::cdf(distribution, value);
}
