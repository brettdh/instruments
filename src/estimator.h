#ifndef ESTIMATOR_H_INCL
#define ESTIMATOR_H_INCL

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
    
    void addObservation(double value);
    void setOwner(EstimatorSet *owner_);
    
    virtual double getEstimate() = 0;
  protected:
    Estimator();

    /* override to get estimates from addObservation. */
    virtual void storeNewObservation(double value) = 0;
    
  private:
    EstimatorSet *owner;
};

/* TODO: Things that are missing from the design right now:
 * 1) Each type of estimator is essentially a singleton.
 *    i.e. there probably needs to be something like an
 *    "estimator registry," even if it's just static methods
 *    on this class.
 * 2) The ExpectationEvaluator subclasses need to iterate over
 *    the stored error values and combine that with the 
 *    Estimator values to feed to the eval function.
 */

#endif
