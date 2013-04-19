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
    AbstractJointDistribution(EmpiricalErrorEvalMethod eval_method_) : eval_method(eval_method_) {}
    virtual ~AbstractJointDistribution() {}

    virtual void setEvalArgs(void *strategy_arg_, void *chooser_arg_) = 0;
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn) = 0;

    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
    virtual void observationAdded(Estimator *estimator, double value) = 0;

    virtual void saveToFile(std::ofstream& out) = 0;
    virtual void restoreFromFile(std::ifstream& in) = 0;
  protected:
    StatsDistribution *createSamplesDistribution();
  private:
    EmpiricalErrorEvalMethod eval_method;
};


#endif
