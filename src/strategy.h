#ifndef STRATEGY_H_INCL
#define STRATEGY_H_INCL

#include "instruments.h"

class Estimator;
class EstimatorSet;

class Strategy {
  public:
    Strategy(eval_fn_t time_fn_, 
             eval_fn_t energy_cost_fn_, 
             eval_fn_t data_cost_fn_, 
             void *fn_arg_);
    
    void addEstimator(Estimator *estimator);
    virtual double calculateTime();
    virtual double calculateCost();
    virtual bool isRedundant() { return false; }

  private:
    eval_fn_t time_fn;
    eval_fn_t energy_cost_fn;
    eval_fn_t data_cost_fn;
    void *fn_arg;

    EstimatorSet *estimators;
};

#endif
