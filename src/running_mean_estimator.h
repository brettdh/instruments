#ifndef RUNNING_MEAN_ESTIMATOR_H_INCL
#define RUNNING_MEAN_ESTIMATOR_H_INCL

#include "estimator.h"

class RunningMeanEstimator : public Estimator {
  public:
    RunningMeanEstimator(const std::string& name);

  protected:
    virtual double getEstimateLocked();
    virtual void storeNewObservation(double value);

  private:
    double mean;
    int count;
};

#endif
