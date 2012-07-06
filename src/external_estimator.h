#ifndef EXTERNAL_ESTIMATOR_H_INCL
#define EXTERNAL_ESTIMATOR_H_INCL

#include "estimator.h"

class ExternalEstimator : public Estimator {
  public:
    ExternalEstimator();
    void addObservation(double observation, double new_value);

    virtual double getEstimate();
  protected:
    virtual void storeNewObservation(double observation);

  private:
    double lastValue;
};

#endif
