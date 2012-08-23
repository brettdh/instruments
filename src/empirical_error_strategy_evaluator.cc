#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "strategy.h"
#include "joint_distribution.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

EmpiricalErrorStrategyEvaluator::EmpiricalErrorStrategyEvaluator()
{
}

void 
EmpiricalErrorStrategyEvaluator::setStrategies(const instruments_strategy_t *strategies_,
                                               size_t num_strategies_)
{
    // TODO: refactor.
    StrategyEvaluator::setStrategies(strategies_, num_strategies_);
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
    jointDistribution->observationAdded(estimator, value);
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    jointDistribution->setEvalArgs(strategy_arg, chooser_arg);
    return jointDistribution->expectedValue(strategy, fn);
}

