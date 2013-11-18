#ifndef EXTERNAL_ESTIMATOR_H_INCL
#define EXTERNAL_ESTIMATOR_H_INCL

#include "estimator.h"

class ExternalEstimator : public Estimator {
  public:
    ExternalEstimator(const std::string& name);
    void addObservation(double observation, double new_value);

  protected:
    virtual double getEstimateLocked();
    virtual void storeNewObservation(double observation);

  private:
    double lastValue;
    double stagedValue;
};

#endif
