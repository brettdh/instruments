#ifndef CONTINUOUS_DISTRIBUTION_H_INCL_9SGFHUBRE
#define CONTINUOUS_DISTRIBUTION_H_INCL_9SGFHUBRE

#include <boost/math/distributions/weibull.hpp>

class ContinuousDistribution {
  public:
    // creates a Weibull distribution.
    ContinuousDistribution(double shape_, double scale_);
    
    // returns the probability that the value is less than upper,
    //   given that it is greater than lower.
    // Pr(X < upper | X >= lower)
    // 
    // note that this is NOT Pr(X >= lower ^ X < upper)
    double getProbabilityValueIsInRange(double lower, double upper);

  private:
    boost::math::weibull_distribution distribution;
};

#endif
