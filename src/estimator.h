#ifndef ESTIMATOR_H_INCL
#define ESTIMATOR_H_INCL

#include <set>
#include "estimator_type.h"

class StrategyEvaluator;

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
    
    void subscribe(StrategyEvaluator *subscriber);
    
    virtual double getEstimate() = 0;
    virtual ~Estimator() {}
  protected:
    Estimator() {}

    /* override to get estimates from addObservation. */
    virtual void storeNewObservation(double value) = 0;
    
  private:
    std::set<StrategyEvaluator*> subscribers;
    const static EstimatorType DEFAULT_TYPE = RUNNING_MEAN;
};

#endif
