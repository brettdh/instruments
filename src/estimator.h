#ifndef ESTIMATOR_H_INCL
#define ESTIMATOR_H_INCL

#include "instruments.h"
#include "estimator_range_hints.h"
#include "estimator_type.h"
#include "small_set.h"

#include <string>
#include <map>

class StrategyEvaluator;

enum ConditionType {
    AT_LEAST = INSTRUMENTS_ESTIMATOR_VALUE_AT_LEAST,
    AT_MOST = INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST
};

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
    static Estimator *create(std::string name);
    static Estimator *create(EstimatorType type, std::string name);
    
    void addObservation(double value);
    
    void subscribe(StrategyEvaluator *subscriber);
    void unsubscribe(StrategyEvaluator *unsubscriber);

    bool hasEstimate();
    virtual double getEstimate() = 0;
    virtual ~Estimator();

    virtual std::string getName();

    bool hasRangeHints();
    EstimatorRangeHints getRangeHints();
    void setRangeHints(double min, double max, size_t num_bins);

    // used to set conditional probability bounds on the value of an estimator
    //   that can be used during strategy evaluation.
    // for instance, if I know the current wifi session is at least
    //   60 seconds long, I can ignore prior error observations
    //   that tell me that it's only 30 seconds long.
    void setCondition(enum ConditionType type, double value);
    void clearConditions();
    bool hasConditions();
    bool valueMeetsConditions(double value);

    // assumes that a bound has been set, and returns that bound.
    // if only one bound is set, returns that bound.
    // if two bounds have been set, returns their midpoint.
    // (This is unlikely in practice, but it's reasonable,
    // since this is only used when no past estimates satisfy
    // the bounds, and if the value is somehow known to be
    // between two bounds, the midpoint is a reasonable guess.
    double getConditionalBound();

    // returns lower bound on estimator if set, or DBL_MIN if none.
    double getLowerBound();

    // returns upper bound on estimator if set, or DBL_MAX if none.
    double getUpperBound();
  protected:
    Estimator(const std::string& name_);

    /* override to get estimates from addObservation. */
    virtual void storeNewObservation(double value) = 0;
    
  private:
    std::string name;
    bool has_estimate;

    pthread_mutex_t subscribers_mutex;
    small_set<StrategyEvaluator*> subscribers;
    const static EstimatorType DEFAULT_TYPE = RUNNING_MEAN;

    bool has_range_hints;
    EstimatorRangeHints range_hints;

    std::map<enum ConditionType, double> conditions;
};

bool estimate_is_valid(double estimate);
double invalid_estimate();

#endif
