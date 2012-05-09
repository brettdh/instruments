#include <math.h>
#include "goal_adaptive_resource_weight.h"
#include "timeops.h"

#include <functional>
using std::min; using std::max;

// TODO: go through and do the trivial Java->C++ conversions
// TODO: figure out what needs to be protected with mutexes

double GoalAdaptiveResourceWeight::PROHIBITIVELY_LARGE_WEIGHT = -1.0;

GoalAdaptiveResourceWeight::GoalAdaptiveResourceWeight(std::string type, double supply, struct timeval goalTime)
{
    if (PROHIBITIVELY_LARGE_WEIGHT < 0.0) {
        // really large, but shouldn't overflow (max ~ 2^1023)
        PROHIBITIVELY_LARGE_WEIGHT = pow(2, 200);
    }

    this->type = type;
    initialSupply = supply;
    this->lastSupply = supply;
    this->goalTime = goalTime;
    TIME(lastResourceUseSample);
        
    lastSpendingRate = supply / secondsUntil(goalTime);
    spendingRateUpdateCount = 1;
                
    // large starting weight: I'll spend my entire budget to save
    //  an amount of time as big as my entire goal.
    this->weight = secondsUntil(goalTime) / supply;
        
    // TODO: redo this using an update thread, or else centralize the call to updateWeight();
    /*
    updateTimer = new Timer();
    updateTask = new TimerTask() {
        @Override
        public void run() {
            updateWeight();
        }
    };

    updateTimer.schedule(updateTask, UPDATE_DURATION_MILLIS, UPDATE_DURATION_MILLIS);
    */
}

void 
GoalAdaptiveResourceWeight::reportSpentResource(double amount)
{
    double samplePeriod = secondsSince(lastResourceUseSample);
    TIME(lastResourceUseSample);

    logPrint("Old %s spending rate: %.6f   old supply: %.6f",
             type.c_str(), lastSpendingRate, lastSupply);
    logPrint("current %s spent amount %.6f over past %.6f seconds",
             type.c_str(), amount, samplePeriod);
        
    double rateSample = amount / samplePeriod;
    double alpha = smoothingFactor();
    lastSpendingRate = calculateNewSpendingRate(lastSpendingRate, rateSample);
    spendingRateUpdateCount++;
    lastSupply -= amount;
        
    logPrint("New %s spending rate: %.6f   new supply: %.6f  (alpha %.6f)",
             type.c_str(), lastSpendingRate, lastSupply, alpha);
}

bool
GoalAdaptiveResourceWeight::supplyIsExhausted()
{
    double adjustedSupply = computeAdjustedSupply(lastSupply);
    return (lastSupply <= 0.0 || adjustedSupply <= 0.0);
}
    
// return the resource cost weight, given the state of the supply and demand.
double
GoalAdaptiveResourceWeight::getWeight()
{
    return weight;
}
    
// return the resource cost weight assuming I incur 'cost' over 'duration' seconds.
// the values passed in here are for current cost, not delta cost.
double 
GoalAdaptiveResourceWeight::getWeight(std::string type, double cost, double duration) 
{
    logPrint("Calculating lookahead %s weight: cost %.6f duration %.6f",
             type.c_str(), cost, duration);
    double spendingRate = 
        calculateNewSpendingRate(lastSpendingRate, lastSpendingRate + (cost / duration));
    logPrint("Lookahead %s spending rate: %.6f", 
             type.c_str(), spendingRate);
    return calculateNewWeight(weight, lastSupply - cost, spendingRate);
}

void
GoalAdaptiveResourceWeight::updateGoalTime(struct timeval newGoalTime)
{
    goalTime = newGoalTime;
}


void
GoalAdaptiveResourceWeight::logPrint(const char *fmt, ...)
{
    // TODO: implement?
}

double 
GoalAdaptiveResourceWeight::smoothingFactor()
{
    // from Odyssey.
    struct timeval now;
    TIME(now);
    if (timercmp(&goalTime, &now, <)) {
        return 0.0;
    } else {
        return pow(2, -1.0 / (0.1 * secondsUntil(this->goalTime)));
    }
}

double
GoalAdaptiveResourceWeight::secondsUntil(struct timeval date)
{
    struct timeval now, diff;
    TIME(now);
    double sign = 1.0;
    if (timercmp(&now, &date, <=)) {
        TIMEDIFF(now, date, diff);
    } else {
        TIMEDIFF(date, now, diff);
        sign = -1.0;
    }
    double seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);
    return sign * seconds;
}
    
double
GoalAdaptiveResourceWeight::secondsSince(struct timeval date)
{
    return -1.0 * secondsUntil(date);
}
    

double
GoalAdaptiveResourceWeight::calculateNewSpendingRate(double oldRate, double rateSample)
{
    if (spendingRateUpdateCount < EWMA_SWITCH_THRESHOLD) {
        // Simple arithmetic mean for the first X updates
        int n = spendingRateUpdateCount;
        return ((oldRate * n) + rateSample) / (n + 1);
    } else {
        double alpha = smoothingFactor();
        return (1 - alpha) * rateSample + alpha * oldRate;
    }
}
    
void
GoalAdaptiveResourceWeight::updateWeight()
{
    weight = calculateNewWeight(weight, lastSupply, lastSpendingRate);
}

double
GoalAdaptiveResourceWeight::calculateNewWeight(double oldWeight, double supply, double spendingRate)
{
    struct timeval now;
    TIME(now);
    double newWeight = oldWeight;
        
    logPrint("Old %s weight: %.6f", type.c_str(), oldWeight);
    // "fudge factor" to avoid overshooting budget.  Borrowed from Odyssey.
    double adjustedSupply = computeAdjustedSupply(supply);
    if (supply <= 0.0 || adjustedSupply <= 0.0) {
        return PROHIBITIVELY_LARGE_WEIGHT;
    } else if (timercmp(&now, &goalTime, >)) {
        // goal reached; spend away!  (We shouldn't see this in our experiments.)
        // weight = 0.0;
        // on second thought, let's try to avoid spending like crazy at the end
        //  due to subtle timing issues.
        return PROHIBITIVELY_LARGE_WEIGHT;
    } else {
        double futureDemand = spendingRate * secondsUntil(goalTime);
        logPrint("%s spending rate: %.6f  adjusted supply: %.6f", 
                 type.c_str(), spendingRate, adjustedSupply);
        logPrint("Future %s demand: %.6f  weight %.6f  multiplier %.6f",
                 type.c_str(), futureDemand, oldWeight, futureDemand / adjustedSupply);
        newWeight *= (futureDemand / adjustedSupply);
    }
    newWeight = max(newWeight, aggressiveNonZeroWeight());
    newWeight = min(newWeight, PROHIBITIVELY_LARGE_WEIGHT); // make sure it doesn't grow without bound
        
    logPrint("New %s weight: %.6f", type.c_str(), newWeight);
    return newWeight;
}

double
GoalAdaptiveResourceWeight::computeAdjustedSupply(double supply)
{
    return supply - (VARIABLE_BUFFER_WEIGHT * supply + CONSTANT_BUFFER_WEIGHT * initialSupply);
}
    
double
GoalAdaptiveResourceWeight::aggressiveNonZeroWeight()
{
    // zero is the most aggressive weight, but if we set weight to zero,
    //  it will never increase after that point.
    // So, instead of using zero as the most aggressive weight,
    //  we use something very small but proportional to the initial supply:
    //  "I would spend my entire budget to save 10ms."
    return 0.01 / initialSupply;
}
