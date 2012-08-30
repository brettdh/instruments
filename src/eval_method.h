#ifndef EVAL_METHOD_H_INCL
#define EVAL_METHOD_H_INCL

#ifdef __cplusplus
extern "C" {
#endif

enum EmpiricalErrorEvalMethod {
    ALL_SAMPLES = 0x0, // default
    BINNED      = 0x1
};

enum JointDistributionType {
    GENERAL_JOINT_DISTRIBUTION = 0x00, // default
    INTNW_JOINT_DISTRIBUTION = 0x10
};

extern const int EMPIRICAL_ERROR_EVAL_METHOD_MASK;
extern const int JOINT_DISTRIBUTION_TYPE_MASK;

enum EvalMethod {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    CONFIDENCE_BOUNDS, // Chebyshev bounds on predictor error
    BAYESIAN,          // Bayesian estimation of posterior 
                       //   estimator distribution

    EMPIRICAL_ERROR=0x100,   // Historical predictor error distribution
    EMPIRICAL_ERROR_ALL_SAMPLES=(EMPIRICAL_ERROR | ALL_SAMPLES),
    EMPIRICAL_ERROR_BINNED=(EMPIRICAL_ERROR | BINNED),
    EMPIRICAL_ERROR_ALL_SAMPLES_INTNW=(EMPIRICAL_ERROR_ALL_SAMPLES | 
                                       INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_BINNED_INTNW=(EMPIRICAL_ERROR_BINNED | 
                                  INTNW_JOINT_DISTRIBUTION)
};

#ifdef __cplusplus
}
#endif

#endif
