#ifndef ESTIMATOR_SET_H_INCL
#define ESTIMATOR_SET_H_INCL

class EstimatorSet {
  public:
    void addEstimator(Estimator *estimator);
    double expectedValue(eval_fn_t fn);
};

#endif
