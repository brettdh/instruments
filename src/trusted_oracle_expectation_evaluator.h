#ifndef TRUSTED_ORACLE_EXPECTATION_EVALUATOR_H_INCL
#define TRUSTED_ORACLE_EXPECTATION_EVALUATOR_H_INCL

#include "estimator_set.h"
#include "expectation_evaluator.h"

class EstimatorSet::TrustedOracleExpectationEvaluator : public EstimatorSet::ExpectationEvaluator {
  public:
    TrustedOracleExpectationEvaluator(EstimatorSet *owner_);

    virtual void startIteration();
    virtual void finishIteration();
    virtual double jointProbability();
    virtual void advance();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
};

#endif
