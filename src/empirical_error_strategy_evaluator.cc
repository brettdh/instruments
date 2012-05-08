#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"

void 
EmpiricalErrorStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (jointError.count(estimator) > 0) {
        double error = estimator->getEstimate() - value;
        jointError[estimator]->addValue(error);
    } else {
        // TODO: move this to a factory method
        jointError[estimator] = new StatsDistributionAllSamples;
    }
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(typesafe_eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;
    EvalContext context(this);
    while (!context.isDone()) {
        weightedSum += fn(&context, arg) * context.jointProbability();
        context.advance();
    }
    return weightedSum;
}

EmpiricalErrorStrategyEvaluator::
EvalContext::EvalContext(EmpiricalErrorStrategyEvaluator *e)
    : evaluator(e)
{
    for (std::map<Estimator*, StatsDistribution *>::const_iterator it = evaluator->jointError.begin();
         it != evaluator->jointError.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution::Iterator *stats_iter = it->second->getIterator();
        iterators[estimator] = stats_iter;
    }
}

EmpiricalErrorStrategyEvaluator::
EvalContext::~EvalContext()
{
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution::Iterator *stats_iter = it->second;
        StatsDistribution *distribution = evaluator->jointError[estimator];
        distribution->finishIterator(stats_iter);
    }
}

double
EmpiricalErrorStrategyEvaluator::
EvalContext::getAdjustedEstimatorValue(Estimator *estimator)
{
    StatsDistribution::Iterator *it = iterators[estimator];
    double error = it->value();
    return estimator->getEstimate() - error;
}

double
EmpiricalErrorStrategyEvaluator::EvalContext::jointProbability()
{
    double probability = 1.0;
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = it->second;
        probability *= stats_iter->probability();
    }
    return probability;
}

EmpiricalErrorStrategyEvaluator::EvalContext::IteratorStack
EmpiricalErrorStrategyEvaluator::EvalContext::setupIteratorStack()
{
    IteratorStack the_stack;
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        the_stack.push(*it);
    }
    return the_stack;
}

void
EmpiricalErrorStrategyEvaluator::EvalContext::advance()
{
    IteratorStack iterator_stack = setupIteratorStack();
    std::pair<Estimator*, StatsDistribution::Iterator *>* top_iter
        = &iterator_stack.top();

    // loop until we actually increment the joint iterator,
    //  or until we realize that it's done.
    Estimator *estimator = top_iter->first;
    StatsDistribution::Iterator *stats_iter = top_iter->second;
    stats_iter->advance();
        
    IteratorStack reset_iterators;
    while (top_iter && stats_iter->isDone()) {
        reset_iterators.push(*top_iter);
        iterator_stack.pop();
        if (!iterator_stack.empty()) {
            top_iter = &iterator_stack.top();
            estimator = top_iter->first;
            stats_iter = top_iter->second;

            // we haven't actually advanced the joint iterator, 
            //  so try advancing the next most significant
            //  estimator error iterator.
            stats_iter->advance();
        } else {
            top_iter = NULL;
        }
    }

    if (!iterator_stack.empty()) {
        // if the joint iteration isn't done yet, 
        //  reset the 'less significant iterators'
        //  than the one that was actually advanced.
        while (!reset_iterators.empty()) {
            top_iter = &reset_iterators.top();
            estimator = top_iter->first;
            stats_iter = top_iter->second;
            StatsDistribution *distribution = evaluator->jointError[estimator];
            distribution->finishIterator(stats_iter);
            iterators[estimator] = distribution->getIterator();

            reset_iterators.pop();
        }
    }
}

bool
EmpiricalErrorStrategyEvaluator::EvalContext::isDone()
{
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = it->second;
        if (!stats_iter->isDone()) {
            return false;
        }
    }
    
    return true;
}
