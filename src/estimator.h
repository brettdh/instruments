#ifndef ESTIMATOR_H_INCL
#define ESTIMATOR_H_INCL

#include <estimator_type.h>

class EstimatorSet;

/* Pure virtual base class for all types of estimators.
 * Takes in values, returns an estimate.
 * Possible ways to implement this:
 * 1) Return last observation
 * 2) Running average
 * 3) EWMA
 * 4) Flip-flop filter
 */
class Estimator {
  public:
    static Estimator *create();
    static Estimator *create(EstimatorType type);
    
    void addObservation(double value);
    void setOwner(EstimatorSet *owner_);
    
    virtual double getEstimate() = 0;
  protected:
    Estimator();

    /* override to get estimates from addObservation. */
    virtual void storeNewObservation(double value) = 0;
    
  private:
    EstimatorSet *owner;
    const static EstimatorType DEFAULT_TYPE = RUNNING_MEAN;
};

#endif
