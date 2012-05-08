#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"

void 
EmpiricalErrorStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (jointError.count(estimator) > 0) {
        double error = estimator->getValue() - value;
        jointError[estimator]->addValue(error);
    } else {
        jointError[estimator] = ErrorDistribution::create(estimator);
    }
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(typesafe_eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;
    EvalContext context(this);
    while (!context.isDone()) {
        sum += fn(&context, arg) * context.jointProbability();
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
    return estimator->getValue() - error;
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
}

IteratorStack
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
    Estimator *estimator = top_iter->first;
    StatsDistribution::Iterator *stats_iter = top_iter->second;
    StatsDistribution *distribution = evaluator->jointError[estimator];
    top_iter->advance();

    while (/* ??? */) {
        // TODO: loop until we actually increment the joint iterator,
        //       or until we realize that it's done.
    }
}

bool
EmpiricalErrorStrategyEvaluator::EvalContext::isDone()
{
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = top_iter->second;
        if (!stats_iter->isDone()) {
            return false;
        }
    }
    
    return true;
}
