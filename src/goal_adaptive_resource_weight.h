#ifndef GOAL_ADAPTIVE_RESOURCE_WEIGHT_H_INCL
#define GOAL_ADAPTIVE_RESOURCE_WEIGHT_H_INCL

#include <sys/time.h>
#include <string>

class GoalAdaptiveResourceWeight {
  public:
    GoalAdaptiveResourceWeight(std::string type, double supply, struct timeval goalTime);
    ~GoalAdaptiveResourceWeight();

    void startPeriodicUpdates();

    void reportSpentResource(double amount);
    bool supplyIsExhausted();
    
    // return the resource cost weight, given the state of the supply and demand.
    double getWeight();
    
    // return the resource cost weight assuming I incur 'cost' over 'duration' seconds.
    // the values passed in here are for current cost, not delta cost.
    double getWeight(std::string type, double cost, double duration);

    void updateGoalTime(struct timeval newGoalTime);

    void updateWeight();
    
  private:
    static const int UPDATE_INTERVAL_SECS = 1;
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
    
    void updateWeightLocked();
    
    double calculateNewWeight(double oldWeight, double supply, double spendingRate);

    static const double VARIABLE_BUFFER_WEIGHT;
    static const double CONSTANT_BUFFER_WEIGHT;

    // adjust the resource supply by an Odyssey-style "fudge factor" to 
    //  make it less likely we'll overshoot the budget.
    double computeAdjustedSupply(double supply);
    
    double aggressiveNonZeroWeight();

    // synchronization stuff
    friend void *WeightUpdateThread(void*);
    pthread_t update_thread;
    pthread_mutex_t mutex;
    pthread_cond_t cv;
    bool updating;
    bool stillUpdating();
    void waitForNextPeriodicUpdate();
};

#endif
