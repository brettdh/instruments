#ifndef EVAL_STRATEGY_H_INCL
#define EVAL_STRATEGY_H_INCL

#ifdef __cplusplus
extern "C" {
#endif

enum EvalStrategy {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    SIMPLE_STATS,      // All-time statistical summary
    EMPIRICAL_ERROR,   // Historical predictor error distribution
    CONFIDENCE_BOUNDS, // Chebyshev bounds on predictor error
    BAYESIAN           // Bayesian estimation of posterior 
                       //   estimator distribution
};

#ifdef __cplusplus
}
#endif

#endif
