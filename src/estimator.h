#ifndef ESTIMATOR_H_INCL
#define ESTIMATOR_H_INCL

#include "estimator_range_hints.h"
#include "estimator_type.h"
#include "small_set.h"

#include <string>

class StrategyEvaluator;

enum ConditionType {
    AT_LEAST,
    AT_MOST
};

/* Pure virtual base class for all types of estimators.{}
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
    bool valueMeetsConditions(double value);
  protected:
    Estimator(const std::string& name_);

    /* override to get estimates from addObservation. */
    virtual void storeNewObservation(double value) = 0;
    
  private:
    std::string name;
    bool has_estimate;

    small_set<StrategyEvaluator*> subscribers;
    const static EstimatorType DEFAULT_TYPE = RUNNING_MEAN;

    bool has_range_hints;
    EstimatorRangeHints range_hints;

    std::map<enum ConditionType, double) conditions;
};

bool estimate_is_valid(double estimate);
double invalid_estimate();

#endif
