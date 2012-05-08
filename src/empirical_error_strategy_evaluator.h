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
    virtual double expectedValue(typesafe_eval_fn_t fn, void *arg);
    
    class EvalContext : public StrategyEvaluationContext {
      public:
        EvalContext(EmpiricalErrorStrategyEvaluator *e);
        ~EvalContext();
        virtual double getAdjustedEstimatorValue(Estimator *estimator);

        double jointProbability();
        void advance();
        bool isDone();
      private:
        typedef std::stack<std::pair<Estimator*, StatsDistribution::Iterator *> > IteratorStack;
        IteratorStack setupIteratorStack();
        
        EmpiricalErrorStrategyEvaluator *evaluator;
        std::map<Estimator*, StatsDistribution::Iterator *> iterators;
    };
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    std::map<Estimator *, StatsDistribution *> jointError;
};

#endif
