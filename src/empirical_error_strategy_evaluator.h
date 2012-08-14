#ifndef EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL
#define EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL

#include <vector>
#include "strategy.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "stats_distribution.h"
#include "small_map.h"

class Estimator;

class JointErrorIterator;
typedef small_map<Estimator *, StatsDistribution *> JointErrorMap;

class EmpiricalErrorStrategyEvaluator : public StrategyEvaluator {
  public:
    EmpiricalErrorStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    friend class SingleStrategyJointErrorIterator;
    
    JointErrorMap jointError;

    JointErrorIterator *jointErrorIterator;
};

#endif
