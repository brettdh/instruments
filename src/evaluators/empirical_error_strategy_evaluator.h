#ifndef EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL
#define EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL

#include "instruments.h"
#include "strategy.h"
#include "strategy_evaluator.h"
#include "eval_method.h"

class Estimator;
class AbstractJointDistribution;

class EmpiricalErrorStrategyEvaluator : public StrategyEvaluator {
  public:
    EmpiricalErrorStrategyEvaluator(EvalMethod eval_method);
    virtual ~EmpiricalErrorStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);

    virtual void saveToFile(const char *filename);
    virtual void restoreFromFile(const char *filename);
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
    virtual void setStrategies(const instruments_strategy_t *strategies_,
                               size_t num_strategies_);

    virtual AbstractJointDistribution *createJointDistribution(JointDistributionType type);
    
    JointDistributionType joint_distribution_type;
  private:
    StatsDistributionType dist_type;
    AbstractJointDistribution *jointDistribution;
};

#endif
