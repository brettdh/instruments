#ifndef ABSTRACT_JOINT_DISTRIBUTION_H_INCL
#define ABSTRACT_JOINT_DISTRIBUTION_H_INCL

#include "eval_method.h"
#include "strategy.h"
#include "strategy_evaluation_context.h"

#include <fstream>

class Estimator;
class StatsDistribution;

class AbstractJointDistribution : public StrategyEvaluationContext {
  public:
    AbstractJointDistribution(StatsDistributionType dist_type_) : dist_type(dist_type_) {}
    virtual ~AbstractJointDistribution() {}

    virtual void setEvalArgs(void *strategy_arg_, void *chooser_arg_) = 0;
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn) = 0;

    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
    virtual void processObservation(Estimator *estimator, double observation, 
                                    double old_estimate, double new_estimate) = 0;
    virtual void processEstimatorConditionsChange(Estimator *estimator) = 0;

    virtual void saveToFile(std::ofstream& out) = 0;
    virtual void restoreFromFile(std::ifstream& in) = 0;
  protected:
    StatsDistribution *createSamplesDistribution(Estimator *estimator=NULL);
  private:
    StatsDistributionType dist_type;
};


#endif
