#ifndef EVAL_METHOD_H_INCL
#define EVAL_METHOD_H_INCL

#ifdef __cplusplus
extern "C" {
#endif

enum EvalMethod {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    EMPIRICAL_ERROR,   // Historical predictor error distribution
    CONFIDENCE_BOUNDS, // Chebyshev bounds on predictor error
    BAYESIAN           // Bayesian estimation of posterior 
                       //   estimator distribution
};

#ifdef __cplusplus
}
#endif

#endif
