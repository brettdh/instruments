#ifndef ESTIMATOR_TYPE_H_INCL
#define ESTIMATOR_TYPE_H_INCL

#ifdef __cplusplus
extern "C" {
#endif

enum EstimatorType {
    LAST_OBSERVATION,
    RUNNING_MEAN,
    EWMA,
    FLIP_FLOP
};

#ifdef __cplusplus
}
#endif

#endif
