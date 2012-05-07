#ifndef STRATEGY_H_INCL
#define STRATEGY_H_INCL

#include <set>
#include "instruments.h"

class Estimator;
class EstimatorSet;
class StrategyEvaluator;

class Strategy {
  public:
    Strategy(eval_fn_t time_fn_, 
             eval_fn_t energy_cost_fn_, 
             eval_fn_t data_cost_fn_, 
             void *fn_arg_);
    
    void addEstimator(Estimator *estimator);
    virtual double calculateTime(StrategyEvaluator *evaluator);
    virtual double calculateCost(StrategyEvaluator *evaluator);
    virtual bool isRedundant() { return false; }

  private:
    friend class StrategyEvaluator;

    eval_fn_t time_fn;
    eval_fn_t energy_cost_fn;
    eval_fn_t data_cost_fn;
    void *fn_arg;

    std::set<Estimator*> estimators;
};

#endif
