#ifndef RUNNING_MEAN_ESTIMATOR_H_INCL
#define RUNNING_MEAN_ESTIMATOR_H_INCL

#include "estimator.h"

class RunningMeanEstimator : public Estimator {
  public:
    RunningMeanEstimator();

    virtual double getEstimate();
  protected:
    virtual void storeNewObservation(double value);

  private:
    double mean;
    int count;
};

#endif
