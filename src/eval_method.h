#ifndef EVAL_METHOD_H_INCL
#define EVAL_METHOD_H_INCL

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

CDECL enum StatsDistributionType {
    ALL_SAMPLES = 0x0, // default
    BINNED      = 0x1
};

CDECL enum JointDistributionType {
    GENERIC_JOINT_DISTRIBUTION = 0x00, // default
    INTNW_JOINT_DISTRIBUTION = 0x10
};

CDECL const int STATS_DISTRIBUTION_TYPE_MASK;
CDECL const int JOINT_DISTRIBUTION_TYPE_MASK;

CDECL enum EvalMethod {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    CONFIDENCE_BOUNDS, // probabilistic bounds on predictor error
    BAYESIAN,          // Bayesian estimation of posterior 
                       //   estimator distribution

    EMPIRICAL_ERROR=0x100,   // Historical predictor error distribution
    EMPIRICAL_ERROR_ALL_SAMPLES=(EMPIRICAL_ERROR | ALL_SAMPLES),
    EMPIRICAL_ERROR_BINNED=(EMPIRICAL_ERROR | BINNED),
    EMPIRICAL_ERROR_ALL_SAMPLES_INTNW=(EMPIRICAL_ERROR_ALL_SAMPLES | 
                                       INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_BINNED_INTNW=(EMPIRICAL_ERROR_BINNED | 
                                  INTNW_JOINT_DISTRIBUTION),
};

CDECL const char *
get_method_name(enum EvalMethod method);

CDECL enum EvalMethod
get_method(const char *method_name);

#ifdef __cplusplus
#include <vector>
#include <string>
std::vector<std::string> get_all_method_names();
#endif

#endif
