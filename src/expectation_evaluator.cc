#include "expectation_evaluator.h"

EstimatorSet::ExpectationEvaluator::ExpectationEvaluator(EstimatorSet *owner_)
    : owner(owner_)
{
}

double
EstimatorSet::ExpectationEvaluator::evaluate(eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;

    Iterator *iter = startIterator();
    while (!iter->isDone()) {
        double prob = iter->jointProbability();
        double value = fn(this, arg);
        weightedSum += (prob * value);
        
        iter->advance();
    }
    finishIterator(iter);
    return weightedSum;
}
