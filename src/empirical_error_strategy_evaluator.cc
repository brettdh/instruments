#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "stats_distribution_binned.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
using std::make_pair;

#include <vector>
using std::vector;

typedef small_map<Estimator*, StatsDistribution::Iterator *> ErrorIteratorMap;

class IteratorStack;

class EmpiricalErrorStrategyEvaluator::JointErrorIterator {
public:
    JointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *s);
    ~JointErrorIterator();
    double getAdjustedEstimatorValue(Estimator *estimator);
    
    double jointProbability();
    void advance();
    bool isDone();

    //StatsDistribution::Iterator *resetErrorIterator(Estimator *estimator);

private:
    IteratorStack *iterator_stack;// TODO: the deque is slow.  make it faster.
    
    EmpiricalErrorStrategyEvaluator *evaluator;

    ErrorIteratorMap iterators;
};

class IteratorStack {
  public:
    IteratorStack(const ErrorIteratorMap& iterator_map, 
                  EmpiricalErrorStrategyEvaluator::JointErrorIterator *jointIterator_);
    void advance();
    bool isDone();
  private:
    vector<Estimator *> estimators;
    vector<StatsDistribution::Iterator *> iterators;

    EmpiricalErrorStrategyEvaluator::JointErrorIterator *jointIterator;
    bool done;

    void resetIteratorsAbove(int stack_top);
};

IteratorStack::IteratorStack(const ErrorIteratorMap& iterator_map,
                             EmpiricalErrorStrategyEvaluator::JointErrorIterator *jointIterator_)
    : jointIterator(jointIterator_), done(false)
{
    for (ErrorIteratorMap::const_iterator it = iterator_map.begin();
         it != iterator_map.end(); ++it) {
        estimators.push_back(it->first);
        iterators.push_back(it->second);
    }
    
    if (iterators.empty()) {
        // unlikely corner case.
        done = true;
    }
}

inline bool
IteratorStack::isDone()
{
    return done;
}

void
IteratorStack::advance()
{
    if (isDone()) {
        return;
    }

    assert(iterators.size() > 0);
    int stack_top = iterators.size() - 1;

    // loop until we actually increment the joint iterator,
    //  or until we realize that it's done.
    StatsDistribution::Iterator *stats_iter = iterators[stack_top];
    stats_iter->advance();
    
    while (stats_iter->isDone()) {
        --stack_top;
        if (stack_top >= 0) {
            stats_iter = iterators[stack_top];

            // we haven't actually advanced the joint iterator, 
            //  so try advancing the next most significant
            //  estimator error iterator.
            stats_iter->advance();
        } else {
            // all iterators are done; the joint iterator is done too.
            done = true;
            break;
        }
    }

    if (stack_top >= 0) {
        // if the joint iteration isn't done yet, 
        //  reset the 'less significant iterators'
        //  than the one that was actually advanced.
        resetIteratorsAbove(stack_top);
    }
}

void
IteratorStack::resetIteratorsAbove(int stack_top)
{
    assert(stack_top >= 0);
    size_t next_reset = stack_top + 1;
    while (next_reset < iterators.size()) {
        iterators[next_reset]->reset();
        
        //Estimator *estimator = estimators[next_reset];
        //iterators[next_reset] = jointIterator->resetErrorIterator(estimator);
        
        ++next_reset;
    }
}

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
        //jointError[estimator] = new StatsDistributionBinned;
        
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        jointError[estimator]->addValue(0.0);
    }
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    double weightedSum = 0.0;

    if (jointErrorIterator != NULL) {
        // TODO: print message and abort; chooseStrategy has been called concurrently.
        assert(0);
    }
    
    //fprintf(stderr, "Calculating expected value...");
    jointErrorIterator = new JointErrorIterator(this, strategy);
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
    //fprintf(stderr, "...done after %d iterations.\n", iterations);
    
    return weightedSum;
}

EmpiricalErrorStrategyEvaluator::
JointErrorIterator::JointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *strategy)
    : evaluator(e)
{
    for (JointErrorMap::const_iterator it = evaluator->jointError.begin();
         it != evaluator->jointError.end(); ++it) {
        Estimator *estimator = it->first;
        if (strategy->usesEstimator(estimator)) {
            StatsDistribution::Iterator *stats_iter = it->second->getIterator();
            iterators[estimator] = stats_iter;
        }
    }
    iterator_stack = new IteratorStack(iterators, this);
}

EmpiricalErrorStrategyEvaluator::
JointErrorIterator::~JointErrorIterator()
{
    delete iterator_stack;

    for (ErrorIteratorMap::iterator it = iterators.begin();
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
    for (ErrorIteratorMap::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = it->second;
        probability *= stats_iter->probability();
    }
    return probability;
}

inline void
EmpiricalErrorStrategyEvaluator::JointErrorIterator::advance()
{
    iterator_stack->advance();
}

inline bool
EmpiricalErrorStrategyEvaluator::JointErrorIterator::isDone()
{
    return iterator_stack->isDone();
}

/*
StatsDistribution::Iterator *
EmpiricalErrorStrategyEvaluator::JointErrorIterator::resetErrorIterator(Estimator *estimator)
{
    StatsDistribution::Iterator *iterator = iterators[estimator];
    iterator->reset();

    StatsDistribution *distribution = evaluator->jointError[estimator];
    distribution->finishIterator(iterator);
    
    iterator = distribution->getIterator();
    iterators[estimator] = iterator;
    return iterator;
}
*/
