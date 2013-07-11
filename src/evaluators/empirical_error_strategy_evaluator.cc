#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "strategy.h"
#include "abstract_joint_distribution.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

#include <fstream>
#include <stdexcept>
#include <sstream>
using std::ifstream; using std::ofstream;
using std::runtime_error;
using std::ostringstream;

#include "generic_joint_distribution.h"
#include "joint_distributions/intnw_joint_distribution.h"

EmpiricalErrorStrategyEvaluator::EmpiricalErrorStrategyEvaluator(EvalMethod method)
{
    jointDistribution = NULL;
    dist_type = StatsDistributionType(method & STATS_DISTRIBUTION_TYPE_MASK);
    joint_distribution_type = JointDistributionType(method & JOINT_DISTRIBUTION_TYPE_MASK);
}

EmpiricalErrorStrategyEvaluator::~EmpiricalErrorStrategyEvaluator()
{
    delete jointDistribution;
}


void 
EmpiricalErrorStrategyEvaluator::setStrategies(const instruments_strategy_t *strategies_,
                                               size_t num_strategies_)
{
    // TODO: refactor.
    StrategyEvaluator::setStrategies(strategies_, num_strategies_);
    delete jointDistribution;

    jointDistribution = createJointDistribution(joint_distribution_type);
}

AbstractJointDistribution *
EmpiricalErrorStrategyEvaluator::createJointDistribution(JointDistributionType joint_distribution_type)
{
    if (joint_distribution_type == INTNW_JOINT_DISTRIBUTION) {
        return new IntNWJointDistribution(dist_type, strategies);
    } else if (joint_distribution_type == GENERIC_JOINT_DISTRIBUTION) {
        return new GenericJointDistribution(dist_type, strategies);
    } else abort();
    // TODO: other specialized eval methods
}

double 
EmpiricalErrorStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    return jointDistribution->getAdjustedEstimatorValue(estimator);
}

void 
EmpiricalErrorStrategyEvaluator::processObservation(Estimator *estimator, double observation, 
                                                    double old_estimate, double new_estimate)
{
    jointDistribution->processObservation(estimator, observation, old_estimate, new_estimate);
}

void 
EmpiricalErrorStrategyEvaluator::processEstimatorConditionsChange(Estimator *estimator)
{
    jointDistribution->processEstimatorConditionsChange(estimator);
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    jointDistribution->setEvalArgs(strategy_arg, chooser_arg);
    return jointDistribution->expectedValue(strategy, fn);
}

void
EmpiricalErrorStrategyEvaluator::saveToFile(const char *filename)
{
    ofstream out(filename);
    if (!out) {
        ostringstream oss;
        oss << "Failed to open " << filename;
        throw runtime_error(oss.str());
    }

    jointDistribution->saveToFile(out);
    out.close();
}

void 
EmpiricalErrorStrategyEvaluator::restoreFromFileImpl(const char *filename)
{
    ifstream in(filename);
    if (!in) {
        ostringstream oss;
        oss << "Failed to open " << filename;
        throw runtime_error(oss.str());
    }

    jointDistribution->restoreFromFile(in);
    in.close();
}
