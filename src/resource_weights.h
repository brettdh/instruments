#ifndef _RESOURCE_WEIGHTS_H_9GVA9HBFEWAVH89udbDe
#define _RESOURCE_WEIGHTS_H_9GVA9HBFEWAVH89udbDe

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

CDECL double get_energy_cost_weight();
CDECL double get_data_cost_weight();

/* cancels any goal-tracking in progress and
 * sets fixed resource cost weights. 
 * On startup, default is fixed weights of zero. */
CDECL void set_fixed_resource_weights(double fixedEnergyWeight,
                                      double fixedDataWeight);

/* starts goal-based resource weight adjustment,
 * removing any previously-set fixed cost weights. 
 * goalTime is absolute epoch-timestamp;
 * goal tracking starts immediately from current time. */
CDECL void set_resource_budgets(struct timeval goalTime, 
                                int energyBudgetMilliJoules,
                                int dataBudgetBytes);

CDECL void report_spent_energy(int energy_spent_mJ);
CDECL void report_spent_data(int data_spent_bytes);

// interface for mocktime-based tracking
CDECL void update_weights_now();

CDECL void start_periodic_updates();

CDECL void logPrint(const char *fmt, ...)
    __attribute__((format(printf, 1, 2)));



#endif /* _RESOURCE_WEIGHTS_H_9GVA9HBFEWAVH89udbDe */
