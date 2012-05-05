#ifndef ESTIMATOR_SET_H_INCL
#define ESTIMATOR_SET_H_INCL

#include <set>
#include "instruments.h"
#include "eval_strategy.h"

class Estimator;

class EstimatorSet {
  public:
    static EstimatorSet *create();
    static EstimatorSet *create(EvalStrategy type);
    
    // An EstimatorSet does not *own* its estimators; it just gets updates from them.
    void addEstimator(Estimator *estimator);
    void observationAdded(Estimator *estimator, double value);
    
    double expectedValue(eval_fn_t fn, void *arg);

    class ExpectationEvaluator;
  private:
    EstimatorSet();
    
    std::set<Estimator*> estimators;
    
    ExpectationEvaluator *evaluator;
    
    // for giving subclasses member access.
    class TrustedOracleExpectationEvaluator;
    class SimpleStatsExpectationEvaluator;
    class EmpiricalErrorExpectationEvaluator;
    class ConfidenceBoundsExpectationEvaluator;
    class BayesianExpectationEvaluator;

    // TODO: change to a better default.
    const static EvalStrategy DEFAULT_EVAL_STRATEGY = TRUSTED_ORACLE;
};

#endif
