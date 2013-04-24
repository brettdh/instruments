#ifndef BAYESIAN_STRATEGY_EVALUATOR_H_INCL
#define BAYESIAN_STRATEGY_EVALUATOR_H_INCL

#include "empirical_error_strategy_evaluator.h"
#include "eval_method.h"

class AbstractJointDistribution;

/* The Bayesian strategy evaluator shares some basic things in common
 * with the brute-force method -- namely, its use of empirical distributions
 * based on the observations and predictions associated with estimators.
 * The differences are contained in the joint distribution delegate class;
 * this subclass therefore needs only to create the appropriate joint distribution.
 * It also enforces that the underlying stats distributions are binned, as
 * is required for this method (as far as I can tell).
 */
class BayesianStrategyEvaluator : public EmpiricalErrorStrategyEvaluator {
  public:
    BayesianStrategyEvaluator(EvalMethod method);
    virtual ~BayesianStrategyEvaluator();
  protected:
    virtual AbstractJointDistribution *createJointDistribution(JointDistributionType type);
};

#endif
