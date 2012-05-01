#ifndef instruments_h_incl
#define instruments_h_incl

#include "instruments_private.h"
#include <sys/types.h>

/* Functions to specify and manipulate strategies */

typedef double (*eval_fn_t)(instruments_context_t, void *);

instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *arg);

void free_strategy(instruments_strategy_t strategy);

/* Choose the strategy (or comibination of strategies)
 *   that maximizes expected (time + cost).
 * Writes chosen strategy handles to chosen_strategies;
 *   returns the number of strategies chosen.
 * Assumptions:
 *   1) Strategies are single-option strategies, not redundant.
 *   2) Time(strategies) = min(Time(strategy_1), Time(strategy_2), ...)
 *   3) Cost(strategies) = Cost(strategy_1) + Cost(strategy_2) + ...
 */
ssize_t
choose_strategy(const instruments_strategy_t *strategies, size_t num_strategies,
                /* OUT */ instruments_strategy_t *chosen_strategies);


/* Functions to get estimator values */
double /* bytes/sec */ network_bandwidth_down(instruments_context_t ctx, const char *iface);
double /* bytes/sec */ network_bandwidth_up(instruments_context_t ctx, const char *iface);
double /*   seconds */ network_rtt(instruments_context_t ctx, const char *iface);
/* ... */

#endif
