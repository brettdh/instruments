#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

class EmpiricalErrorStrategyEvaluator::JointErrorIterator {
public:
    JointErrorIterator(EmpiricalErrorStrategyEvaluator *e);
    ~JointErrorIterator();
    double getAdjustedEstimatorValue(Estimator *estimator);
    
    double jointProbability();
    void advance();
    bool isDone();
private:
    typedef std::stack<std::pair<Estimator*, StatsDistribution::Iterator *> > IteratorStack;
    IteratorStack setupIteratorStack();
    
    EmpiricalErrorStrategyEvaluator *evaluator;
    std::map<Estimator*, StatsDistribution::Iterator *> iterators;
};

EmpiricalErrorStrategyEvaluator::EmpiricalErrorStrategyEvaluator()
    : jointErrorIterator(NULL)
{
}

double 
EmpiricalErrorStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    assert(jointErrorIterator != NULL);
    return jointErrorIterator->getAdjustedEstimatorValue(estimator);
}

void 
EmpiricalErrorStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (jointError.count(estimator) > 0) {
        double error = estimator->getEstimate() - value;
        jointError[estimator]->addValue(error);
    } else {
        // TODO: move this to a factory method (with the other methods)
        jointError[estimator] = new StatsDistributionAllSamples;
        
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        jointError[estimator]->addValue(0.0);
    }
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    double weightedSum = 0.0;

    if (jointErrorIterator != NULL) {
        // TODO: print message and abort; chooseStrategy has been called concurrently.
        assert(0);
    }
    
    fprintf(stderr, "Calculating expected value...");
    jointErrorIterator = new JointErrorIterator(this);
    int iterations = 0;
    while (!jointErrorIterator->isDone()) {
        double value = fn(this, strategy_arg, chooser_arg);
        double probability = jointErrorIterator->jointProbability();
        weightedSum += value * probability;

        //fprintf(stderr, "value: %f  prob: %f  weightedSum: %f\n",
        //        value, probability, weightedSum);
        jointErrorIterator->advance();
        //sleep(5);
        
        iterations++;
    }
    delete jointErrorIterator;
    jointErrorIterator = NULL;
    fprintf(stderr, "...done after %d iterations.\n", iterations);
    
    return weightedSum;
}

EmpiricalErrorStrategyEvaluator::
JointErrorIterator::JointErrorIterator(EmpiricalErrorStrategyEvaluator *e)
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
JointErrorIterator::~JointErrorIterator()
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
JointErrorIterator::getAdjustedEstimatorValue(Estimator *estimator)
{
    StatsDistribution::Iterator *it = iterators[estimator];
    double error = it->value();
    return estimator->getEstimate() - error;
}

double
EmpiricalErrorStrategyEvaluator::JointErrorIterator::jointProbability()
{
    double probability = 1.0;
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = it->second;
        probability *= stats_iter->probability();
    }
    return probability;
}

EmpiricalErrorStrategyEvaluator::JointErrorIterator::IteratorStack
EmpiricalErrorStrategyEvaluator::JointErrorIterator::setupIteratorStack()
{
    IteratorStack the_stack;
    for (std::map<Estimator*, StatsDistribution::Iterator *>::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        the_stack.push(*it);
    }
    return the_stack;
}

void
EmpiricalErrorStrategyEvaluator::JointErrorIterator::advance()
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
EmpiricalErrorStrategyEvaluator::JointErrorIterator::isDone()
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
