#ifndef EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL
#define EMPIRICAL_ERROR_STRATEGY_EVALUATOR_H_INCL

#include <vector>
#include <map>
#include <stack>
#include "strategy.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "stats_distribution.h"

class Estimator;

class EmpiricalErrorStrategyEvaluator : public StrategyEvaluator {
  public:
    EmpiricalErrorStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    
    class JointErrorIterator;
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    std::map<Estimator *, StatsDistribution *> jointError;

    JointErrorIterator *jointErrorIterator;
};

#endif
