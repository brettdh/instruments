#ifndef STRATEGY_H_INCL
#define STRATEGY_H_INCL

#include <set>
#include <vector>
#include "instruments.h"

class Estimator;
class EstimatorSet;
class StrategyEvaluator;
class StrategyEvaluationContext;

// use this internally to avoid nasty vtable issues caused by
//  casting to void* from a pointer to a polymorphic subtype.
// (We don't care about the type of arg; it's from the application,
//  which must manage its own type casting.)
typedef double (*typesafe_eval_fn_t)(StrategyEvaluationContext *, void *arg);

class Strategy {
  public:
    Strategy(eval_fn_t time_fn_, 
             eval_fn_t energy_cost_fn_, 
             eval_fn_t data_cost_fn_, 
             void *fn_arg_);
    Strategy(const instruments_strategy_t strategies[], 
             size_t num_strategies);
    
    void addEstimator(Estimator *estimator);
    double calculateTime(StrategyEvaluator *evaluator);
    double calculateCost(StrategyEvaluator *evaluator);
    bool isRedundant();

  private:
    friend class StrategyEvaluator;
    friend class EmpiricalErrorStrategyEvaluatorTest;

    friend double redundant_strategy_minimum_time(StrategyEvaluationContext *ctx, void *arg);
    friend double redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, void *arg);
    friend double redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, void *arg);

    typesafe_eval_fn_t time_fn;
    typesafe_eval_fn_t energy_cost_fn;
    typesafe_eval_fn_t data_cost_fn;
    void *fn_arg;

    std::set<Estimator*> estimators;

    std::vector<Strategy *> child_strategies;
};

#endif
