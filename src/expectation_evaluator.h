#ifndef EXPECTATION_EVALUATOR_H_INCL
#define EXPECTATION_EVALUATOR_H_INCL

#include "instruments.h"
#include "estimator_set.h"

/*
 * These help implement the different uncertainty evaluation methods
 *   by deciding how to return estimator values.
 */
class EstimatorSet::ExpectationEvaluator {
  public:
    virtual double evaluate(eval_fn_t fn, void *arg);
    virtual void observationAdded(Estimator *estimator, double value) = 0;
    
    virtual EstimatorSet::Iterator *startIterator() = 0;
    virtual void finishIterator(EstimatorSet::Iterator *iter);
  protected:
    ExpectationEvaluator(EstimatorSet *owner_);
  private:
    EstimatorSet *owner;
};

#endif
