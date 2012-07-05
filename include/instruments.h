#ifndef instruments_h_incl
#define instruments_h_incl

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

/* Functions to specify and manipulate strategies */

/** Opaque handle representing strategy evaluation context. */
typedef void * instruments_context_t;

/** Opaque handle representing an application's strategy
 *  for accomplishing its task and the calculations
 *  of time, energy, and data cost of that strategy. 
 */
typedef void * instruments_strategy_t;

/** Function pointer type for all strategy-evaluation callbacks.
 *  The second argument is the argument supplied in make_strategy.
 * Requirements:
 *  eval functions must not have side effects.
 *  eval functions must not store their first argument
 *    for later use; they should consider it invalid
 *    after they return.
 */
typedef double (*eval_fn_t)(instruments_context_t, void *);

/** Opaque handle representing a 'set' of strategies
 *  that the application has registered and can ask
 *  the framework to choose between. 
 */
typedef void * instruments_strategy_evaluator_t;

/** 
 */
instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *arg);

instruments_strategy_t
make_redundant_strategy(const instruments_strategy_t *strategies, 
                        size_t num_strategies);

void free_strategy(instruments_strategy_t strategy);

/** Register a set of strategies with the framework and obtain
 *  a handle to an evaluator, which can be used later with
 *  choose_strategy.
 *  strategies contains all interested strategies, singular and redundant.
 *  (see make_strategy and make_redundant_strategy above)
 */
instruments_strategy_evaluator_t
register_strategy_set(const instruments_strategy_t *strategies, size_t num_strategies);

void free_strategy_evaluator(instruments_strategy_evaluator_t evaluator);

/** Choose and return the best strategy.
 */
instruments_strategy_t
choose_strategy(instruments_strategy_evaluator_t strategies);
/* TODO: add 'excludes' for when a strategy is not possible? 
 *       or else the opposite? 
 *       or maybe a 'disable_strategy' function that prevents
 *       choose_strategy from considering a strategy until
 *       'enable_strategy' is called? 
 * NOTE: this only matters if there are more than two 
 *        'singular' strategies. 
 */

/* TODO: need an interface for re-evaluation given new information. */



/** Opaque handle representing a quantity that the application
 *  uses to calculate the time and cost of its strategies.
 */
typedef void * instruments_estimator_t;

/** Returns an estimator for the downstream bandwidth 
 *  of a network (named by 'iface') in bytes/sec.
 */
instruments_estimator_t 
get_network_bandwidth_down_estimator(instruments_context_t ctx, const char *iface);

/** Returns an estimator for the upstream bandwidth 
 *  of a network (named by 'iface') in bytes/sec.
 */
instruments_estimator_t
get_network_bandwidth_up_estimator(instruments_context_t ctx, const char *iface);

/** Returns an estimator for the round-trip time
 *  of a network (named by 'iface').
 */
instruments_estimator_t
get_network_rtt_estimator(instruments_context_t ctx, const char *iface);

/* ... */

/** Given an estimator (obtained from one of the above functions),
 *  return its value.  Used in eval functions (obtaining an estimator handle
 *  requires a context, which is only available in eval functions).
 */
double get_estimator_value(instruments_estimator_t estimator);

#ifdef __cplusplus
}
#endif

#endif
