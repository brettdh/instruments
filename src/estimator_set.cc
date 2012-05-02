#include "estimator_set.h"
#include "expectation_evaluator.h"

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

EstimatorSet::ExpectationEvaluator::ExpectationEvaluator(EstimatorSet *owner_)
    : owner(owner_)
{
}

double
EstimatorSet::ExpectationEvaluator::evaluate(eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;

    Iterator *iter = startIterator();
    while (!iter->done()) {
	double prob = iter->jointProbability();
	double value = fn(this, arg);
	weightedSum += (prob * value);
	
	iter->advance();
    }
    finishIterator(iter);
    return weightedSum;
}
