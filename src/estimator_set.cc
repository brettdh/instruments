#include "estimator_set.h"
#include "expectation_evaluator.h"
#include "trusted_oracle_expectation_evaluator.h"

EstimatorSet *
EstimatorSet::create(EvalStrategy type)
{
    EstimatorSet *theSet = new EstimatorSet;

    // TODO: switch the ExpectationEvaluator type based on the argument.
    theSet->evaluator = new TrustedOracleExpectationEvaluator(theSet);

    return theSet;
}

EstimatorSet::EstimatorSet()
    : evaluator(NULL)
{
}

void
EstimatorSet::addEstimator(Estimator *estimator)
{
    estimators.insert(estimator);
}

void
EstimatorSet::observationAdded(Estimator *estimator, double value)
{
    evaluator->observationAdded(estimator, value);
}

double
EstimatorSet::expectedValue(eval_fn_t fn, void *arg)
{
    return evaluator->evaluate(fn, arg);
}
