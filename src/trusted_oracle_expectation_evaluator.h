#ifndef TRUSTED_ORACLE_EXPECTATION_EVALUATOR
#define TRUSTED_ORACLE_EXPECTATION_EVALUATOR

#include "estimator_set.h"

class EstimatorSet::TrustedOoracleExpectationEvaluator : public EstimatorSet::ExpectationEvaluator {
  public:
    virtual void startIteration();
    virtual void finishIteration();
    virtual double jointProbability();
    virtual void advance();
  protected:
    TrustedOracleExpectationEvaluator(EstimatorSet *owner_);
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
