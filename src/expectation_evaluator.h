#ifndef EXPECTATION_EVALUATOR_H_INCL
#define EXPECTATION_EVALUATOR_H_INCL

#include "instruments.h"
#include "estimator_set.h"
#include "estimator.h"

/*
 * These help implement the different uncertainty evaluation methods
 *   by deciding how to return estimator values.
 */
class EstimatorSet::ExpectationEvaluator {
  public:
    virtual double evaluate(eval_fn_t fn, void *arg);
    virtual void observationAdded(Estimator *estimator, double value) = 0;
    
    virtual void startIteration() = 0;
    virtual void finishIteration() = 0;
    virtual double jointProbability() = 0;
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
    virtual void advance() = 0;
    bool isDone();
  protected:
    ExpectationEvaluator(EstimatorSet *owner_);
    bool done;
  private:
    EstimatorSet *owner;
};

#endif
