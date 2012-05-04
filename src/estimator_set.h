#ifndef ESTIMATOR_SET_H_INCL
#define ESTIMATOR_SET_H_INCL

#include <set>
#include "instruments.h"

class Estimator;

class EstimatorSet {
  public:
    enum EvalStrategy {
        TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
        SIMPLE_STATS,      // All-time statistical summary
        EMPIRICAL_ERROR,   // Historical predictor error distribution
        CONFIDENCE_BOUNDS, // Chebyshev bounds on predictor error
        BAYESIAN           // Bayesian estimation of posterior 
                           //   estimator distribution
    };

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
};

#endif
