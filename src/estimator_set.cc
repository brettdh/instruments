#include "estimator_set.h"

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

EstimatorSet::Iterator::Iterator()
    : done(false)
{
}
