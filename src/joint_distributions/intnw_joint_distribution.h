#ifndef _INTNW_JOINT_DISTRIBUTION_H_
#define _INTNW_JOINT_DISTRIBUTION_H_

#include "joint_distribution.h"

class IntNWJointDistribution : public AbstractJointDistribution {
  public:
    IntNWJointDistribution(const std::vector<Strategy *>& strategies);

    virtual void setEvalArgs(void *strategy_arg_, void *chooser_arg_);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    
};

#endif /* _INTNW_JOINT_DISTRIBUTION_H_ */
