#ifndef GOAL_ADAPTIVE_RESOURCE_WEIGHT_H_INCL
#define GOAL_ADAPTIVE_RESOURCE_WEIGHT_H_INCL

#include <sys/time.h>
#include <string>

class GoalAdaptiveResourceWeight {
  public:
    GoalAdaptiveResourceWeight(std::string type, double supply, struct timeval goalTime);
    void reportSpentResource(double amount);
    bool supplyIsExhausted();
    
    // return the resource cost weight, given the state of the supply and demand.
    double getWeight();
    
    // return the resource cost weight assuming I incur 'cost' over 'duration' seconds.
    // the values passed in here are for current cost, not delta cost.
    double getWeight(std::string type, double prefetchCost, double prefetchDuration);

    void updateGoalTime(struct timeval newGoalTime);

  private:
    static const int UPDATE_DURATION_MILLIS = 1000;
    static double PROHIBITIVELY_LARGE_WEIGHT;
    
    static const int EWMA_SWITCH_THRESHOLD = 100;
    double lastSupply;
    double initialSupply;
    struct timeval goalTime;
    double weight;
    
    double lastSpendingRate;
    struct timeval lastResourceUseSample;
    std::string type;
    int spendingRateUpdateCount;
    
    void logPrint(const char *fmt, ...)
        __attribute__((format(printf, 2, 3)));;
    
    double smoothingFactor();
    double secondsUntil(struct timeval date);
    
    double secondsSince(struct timeval date);
    double calculateNewSpendingRate(double oldRate, double rateSample);
    
    void updateWeight();
    
    double calculateNewWeight(double oldWeight, double supply, double spendingRate);

    static const double VARIABLE_BUFFER_WEIGHT = 0.05;
    //static const double CONSTANT_BUFFER_WEIGHT = 0.01;
    
    // trying a higher constant factor because of the artificially short experiment.
    // This is based on taking 1% of a 2-hour experiment's starting supply.
    //  If X is the 15-minute supply and 8X is the 2-hour supply,
    //  then the fudge factor is 0.01 * 8X, or 0.08X.
    static const double CONSTANT_BUFFER_WEIGHT = 0.08;

    // adjust the resource supply by an Odyssey-style "fudge factor" to 
    //  make it less likely we'll overshoot the budget.
    double computeAdjustedSupply(double supply);
    
    double aggressiveNonZeroWeight();
};

#endif
