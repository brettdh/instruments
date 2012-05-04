#include "expectation_evaluator.h"

EstimatorSet::ExpectationEvaluator::ExpectationEvaluator(EstimatorSet *owner_)
    : done(false), owner(owner_)
{
}

double
EstimatorSet::ExpectationEvaluator::evaluate(eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;

    startIteration();
    while (!isDone()) {
        double prob = jointProbability();
        double value = fn(this, arg);
        weightedSum += (prob * value);
        
        advance();
    }
    finishIteration();
    return weightedSum;
}

bool
EstimatorSet::ExpectationEvaluator::isDone()
{
    return done;
}
