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
    eval_method = EmpiricalErrorEvalMethod(method & EMPIRICAL_ERROR_EVAL_METHOD_MASK);
    joint_distribution_type = JointDistributionType(method & JOINT_DISTRIBUTION_TYPE_MASK);
}

void 
EmpiricalErrorStrategyEvaluator::setStrategies(const instruments_strategy_t *strategies_,
                                               size_t num_strategies_)
{
    // TODO: refactor.
    StrategyEvaluator::setStrategies(strategies_, num_strategies_);
    jointDistribution = createJointDistribution();
}

AbstractJointDistribution *
EmpiricalErrorStrategyEvaluator::createJointDistribution()
{
    if (joint_distribution_type == INTNW_JOINT_DISTRIBUTION) {
        return new IntNWJointDistribution(eval_method, strategies);
    } else if (joint_distribution_type == GENERIC_JOINT_DISTRIBUTION) {
        return new GenericJointDistribution(eval_method, strategies);
    } else abort();
    // TODO: other specialized eval methods
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
EmpiricalErrorStrategyEvaluator::restoreFromFile(const char *filename)
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
