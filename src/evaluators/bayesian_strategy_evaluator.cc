#include "bayesian_strategy_evaluator.h"
#include "bayesian_intnw_posterior_distribution.h"
#include "estimator.h"

#include <stdlib.h>

BayesianStrategyEvaluator::BayesianStrategyEvaluator(EvalMethod method)
    : EmpiricalErrorStrategyEvaluator(method), simple_evaluator(NULL)
{
}

BayesianStrategyEvaluator::~BayesianStrategyEvaluator()
{
}


AbstractJointDistribution *
BayesianStrategyEvaluator::createJointDistribution(JointDistributionType joint_distribution_type)
{
    // the "joint" distribution here is really the Bayesian posterior distribution
    if (joint_distribution_type == INTNW_JOINT_DISTRIBUTION) {
        return new BayesianIntNWPosteriorDistribution(strategies);
    } else abort();
    // TODO: other specialized eval methods
}

void
BayesianStrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                         size_t num_strategies)
{
    StrategyEvaluator::setStrategies(new_strategies, num_strategies);
    if (!simple_evaluator) {
        simple_evaluator = new TrustedOracleStrategyEvaluator;
    }
    simple_evaluator->setStrategies(new_strategies, num_strategies);
}

Strategy *
BayesianStrategyEvaluator::getBestSingularStrategy(void *chooser_arg)
{
    return simple_evaluator->chooseStrategy(chooser_arg);
}
