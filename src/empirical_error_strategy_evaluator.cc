#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "strategy.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#ifndef ANDROID
#include "stats_distribution_binned.h"
#endif
#include "multi_dimension_array.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

EmpiricalErrorStrategyEvaluator::EmpiricalErrorStrategyEvaluator()
{
    jointDistribution = new JointDistribution(strategies);
}

double 
EmpiricalErrorStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    return jointDistribution->getAdjustedEstimatorValue(estimator);
}

void 
EmpiricalErrorStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    jointDistribution->observationAdded(Estimator *estimator, double value);
    /*
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
    */
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    return jointDistribution->expectedValue(strategy, fn, strategy_arg, chooser_arg);

    /*
    jointErrorIterator = new SingleStrategyJointErrorIterator(this, strategy);
    while (!jointErrorIterator->isDone()) {
        double value = fn(jointErrorIterator, strategy_arg, chooser_arg);
        double probability = jointErrorIterator->probability();
        weightedSum += value * probability;
        
        jointErrorIterator->advance();
    }
    delete jointErrorIterator;
    jointErrorIterator = NULL;
    
    return weightedSum;
    */
}

