#ifndef LAST_OBSERVATION_ESTIMATOR_H_INCL
#define LAST_OBSERVATION_ESTIMATOR_H_INCL

#include "estimator.h"

class LastObservationEstimator : public Estimator {
  public:
    LastObservationEstimator(const std::string& name) 
        : Estimator(name), lastValue(0.0) {}

  protected:
    virtual double getEstimateLocked() {
        return lastValue;
    }
    virtual void storeNewObservation(double value) {
        lastValue = value;
    }

  private:
    double lastValue;
};

#endif
