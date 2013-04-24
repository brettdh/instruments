#ifndef EVAL_METHOD_H_INCL
#define EVAL_METHOD_H_INCL

#ifdef __cplusplus
extern "C" {
#endif

enum StatsDistributionType {
    ALL_SAMPLES = 0x0, // default
    BINNED      = 0x1
};

enum JointDistributionType {
    GENERIC_JOINT_DISTRIBUTION = 0x00, // default
    INTNW_JOINT_DISTRIBUTION = 0x10
};

extern const int STATS_DISTRIBUTION_TYPE_MASK;
extern const int JOINT_DISTRIBUTION_TYPE_MASK;

enum EvalMethod {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    CONFIDENCE_BOUNDS, // probabilistic bounds on predictor error

    EMPIRICAL_ERROR=0x100,   // Historical predictor error distribution
    EMPIRICAL_ERROR_ALL_SAMPLES=(EMPIRICAL_ERROR | ALL_SAMPLES),
    EMPIRICAL_ERROR_BINNED=(EMPIRICAL_ERROR | BINNED),
    EMPIRICAL_ERROR_ALL_SAMPLES_INTNW=(EMPIRICAL_ERROR_ALL_SAMPLES | 
                                       INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_BINNED_INTNW=(EMPIRICAL_ERROR_BINNED | 
                                  INTNW_JOINT_DISTRIBUTION),

    BAYESIAN=(0x200 | BINNED), // Bayesian estimation of posterior 
                               //   estimator distribution
    BAYESIAN_INTNW=(BAYESIAN | INTNW_JOINT_DISTRIBUTION),
};

const char *
get_method_name(enum EvalMethod method);

#ifdef __cplusplus
}
#endif

#endif
