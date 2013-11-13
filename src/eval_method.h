#ifndef EVAL_METHOD_H_INCL
#define EVAL_METHOD_H_INCL

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

CDECL enum StatsDistributionType {
    ALL_SAMPLES = 0x0, // default
    ALL_SAMPLES_WEIGHTED = 0x1,
    BINNED      = 0x2,
};

CDECL enum JointDistributionType {
    GENERIC_JOINT_DISTRIBUTION = 0x00, // default
    INTNW_JOINT_DISTRIBUTION = 0x10,
    REMOTE_EXEC_JOINT_DISTRIBUTION = 0x20
};

#ifdef __cplusplus
CDECL const int STATS_DISTRIBUTION_TYPE_MASK;
CDECL const int JOINT_DISTRIBUTION_TYPE_MASK;
#else
extern const int STATS_DISTRIBUTION_TYPE_MASK;
extern const int JOINT_DISTRIBUTION_TYPE_MASK;
#endif

CDECL enum EvalMethod {
    TRUSTED_ORACLE,    // No error evaluation; estimators assumed perfect
    CONFIDENCE_BOUNDS, // probabilistic bounds on predictor error
    CONFIDENCE_BOUNDS_WEIGHTED,
    BAYESIAN,          // Bayesian estimation of posterior 
                       //   estimator distribution
    BAYESIAN_WEIGHTED,

    EMPIRICAL_ERROR=0x100,   // Historical predictor error distribution
    EMPIRICAL_ERROR_ALL_SAMPLES=(EMPIRICAL_ERROR | ALL_SAMPLES),
    EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED=(EMPIRICAL_ERROR | ALL_SAMPLES_WEIGHTED),
    EMPIRICAL_ERROR_BINNED=(EMPIRICAL_ERROR | BINNED),
    EMPIRICAL_ERROR_ALL_SAMPLES_INTNW=(EMPIRICAL_ERROR_ALL_SAMPLES | 
                                       INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED_INTNW=(EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED |
                                                INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_BINNED_INTNW=(EMPIRICAL_ERROR_BINNED | 
                                  INTNW_JOINT_DISTRIBUTION),
    EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED_REMOTE_EXEC=(EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED |
                                                      REMOTE_EXEC_JOINT_DISTRIBUTION),
};

CDECL const char *
get_method_name(enum EvalMethod method);

CDECL enum EvalMethod
get_method(const char *method_name);

#ifdef __cplusplus
#include <set>
#include <string>
std::set<std::string> get_all_method_names();
#endif

#endif
