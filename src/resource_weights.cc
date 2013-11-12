#include "resource_weights.h"
#include "goal_adaptive_resource_weight.h"
#include <pthread.h>
#include "pthread_util.h"
#include "mocktime.h"
#include "debug.h"

#include <sstream>
using std::ostringstream;

#include <stdarg.h>

static double fixed_energy_weight = 0.0;
static double fixed_data_weight = 0.0;

static GoalAdaptiveResourceWeight *adaptive_energy_weight = NULL;
static GoalAdaptiveResourceWeight *adaptive_data_weight = NULL;

static pthread_mutex_t weights_lock = MY_PTHREAD_MUTEX_INITIALIZER;

// must be holding weights_lock.
static double 
choose_weight(GoalAdaptiveResourceWeight *adaptive_weight,
              double fixed_weight)
{
    if (adaptive_weight) {
        return adaptive_weight->getWeight(); // TODO: future-looking version
    } else {
        return fixed_weight;
    }
}

double get_energy_cost_weight()
{
    PthreadScopedLock lock(&weights_lock);
    return choose_weight(adaptive_energy_weight, fixed_energy_weight);
}

double get_data_cost_weight()
{
    PthreadScopedLock lock(&weights_lock);
    return choose_weight(adaptive_data_weight, fixed_data_weight);
}

// must be holding weights_lock.
static void clear_adaptive_weights()
{
    delete adaptive_energy_weight;
    delete adaptive_data_weight;
    adaptive_energy_weight = adaptive_data_weight = NULL;
}

/* cancels any goal-tracking in progress and
 * sets fixed resource cost weights. 
 * On startup, default is fixed weights of zero. */
void set_fixed_resource_weights(double energy, double data)
{
    PthreadScopedLock lock(&weights_lock);

    clear_adaptive_weights();

    fixed_energy_weight = energy;
    fixed_data_weight = data;
}

/* starts goal-based resource weight adjustment,
 * removing any previously-set fixed cost weights. 
 * goalTime is absolute epoch-timestamp;
 * goal tracking starts immediately from current time. */
void set_resource_budgets(struct timeval goalTime, 
                          int energyBudgetMilliJoules,
                          int dataBudgetBytes)
{
    PthreadScopedLock lock(&weights_lock);

    clear_adaptive_weights();
    
    logPrint("Setup adaptive strategy\n");

    adaptive_energy_weight = 
        new GoalAdaptiveResourceWeight("energy", energyBudgetMilliJoules,
                                       goalTime);
    adaptive_data_weight = 
        new GoalAdaptiveResourceWeight("data", dataBudgetBytes,
                                       goalTime);

}

void start_periodic_updates()
{
    PthreadScopedLock lock(&weights_lock);
    ASSERT(adaptive_energy_weight && adaptive_data_weight);
    adaptive_energy_weight->startPeriodicUpdates();
    adaptive_data_weight->startPeriodicUpdates();
}

static void 
report_spent_resource(GoalAdaptiveResourceWeight *weight, double amount)
{
    if (weight) {
        weight->reportSpentResource(amount);
    }
}

void report_spent_energy(int energy_spent_mJ)
{
    PthreadScopedLock lock(&weights_lock);
    report_spent_resource(adaptive_energy_weight, energy_spent_mJ);
}

void report_spent_data(int data_spent_bytes)
{
    PthreadScopedLock lock(&weights_lock);
    report_spent_resource(adaptive_data_weight, data_spent_bytes);
}

void update_weights_now()
{
    PthreadScopedLock lock(&weights_lock);
    if (adaptive_energy_weight && adaptive_data_weight) {
        adaptive_energy_weight->updateWeight();
        adaptive_data_weight->updateWeight();
    }
}

void
logPrint(const char *fmt, ...)
{
    static const char *LOGFILE_NAME = "/tmp/budgets.log";
    FILE *logfile = fopen(LOGFILE_NAME, "a");
    if (logfile) {
        ostringstream oss;
        struct timeval now;
        mocktime_gettimeofday(&now, NULL);
        oss << ((now.tv_sec * 1000) + (now.tv_usec / 1000))
            << " " << fmt;

        va_list ap;
        va_start(ap, fmt);
        vfprintf(logfile, oss.str().c_str(), ap);
        va_end(ap);
        
        fclose(logfile);
    }
}
