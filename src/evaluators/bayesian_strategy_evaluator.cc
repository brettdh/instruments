#include "bayesian_strategy_evaluator.h"
#include "bayesian_intnw_posterior_distribution.h"
#include "estimator.h"

#include <stdlib.h>

BayesianStrategyEvaluator::BayesianStrategyEvaluator(EvalMethod method)
    : EmpiricalErrorStrategyEvaluator(method)
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
