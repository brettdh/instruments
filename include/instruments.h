#ifndef instruments_h_incl
#define instruments_h_incl

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

/**
 * @file instruments.h
 *
 * Interface functions for evaluating multiple execution strategies.
 */

#include <sys/types.h>
#include <sys/time.h>

/* Functions to specify and manipulate strategies */

/** 
 * Opaque handle representing strategy evaluation context. 
 */
typedef void * instruments_context_t;

/** 
 *  Opaque handle representing an application's strategy
 *  for accomplishing its task and the calculations
 *  of time, energy, and data cost of that strategy. 
 */
typedef void * instruments_strategy_t;

/** Function pointer type for all strategy-evaluation callbacks.
 *  The second argument is the argument supplied in make_strategy.
 *  The third argument is the argument supplied in choose_strategy.
 * Requirements:
 *  eval functions must not have side effects.
 *  eval functions must not store their first argument
 *    for later use; they should consider it invalid
 *    after they return.
 */
typedef double (*eval_fn_t)(instruments_context_t, void *, void *);

/** Opaque handle representing a 'set' of strategies
 *  that the application has registered and can ask
 *  the framework to choose between. 
 */
typedef void * instruments_strategy_evaluator_t;

/** Create a 'strategy' to be evaluated against other strategies.
 *  
 *  A strategy represents one of many possible ways
 *  of accomplishing a given task - for example, transferring
 *  bytes to a remote host.  In order to make a decision about which
 *  strategy to use, Instruments must be able to estimate the 
 *  completion time, energy cost, and cellular data cost of
 *  each strategy.
 *  To this end, a strategy consists of three callbacks:
 *  1) time_fn
 *     Returns the estimated completion time in seconds.
 *  2) energy_cost_fn
 *     Returns the estimated energy expenditure in Joules.
 *  3) data_cost_fn
 *     Returns the number of bytes to be sent on a cellular network.
 *  
 *  Callers may optionally specify an argument 'strategy_arg' that will be passed
 *  to each evaluation function when it is invoked.
 *  Further, when calling choose_strategy, its optional argument
 *  will be passed as the last argument to the callbacks.
 *  If the callbacks make use of chooser_arg (as passed into choose_strategy),
 *  they should here supply an acceptable 'default' argument,
 *  so the framework may call their callbacks (for introspection purposes)
 *  without having a 'real' argument here.
 *
 *  (XXX: this awkwardness could perhaps be obsoleted by
 *   reworking estimator discovery as static (rather than dynamic) analysis.)
 */
CDECL instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *strategy_arg,
              void *default_chooser_arg);

/** Create a *redundant* strategy by combining two or more
 *  single-option strategies.
 */
CDECL instruments_strategy_t
make_redundant_strategy(const instruments_strategy_t *strategies, 
                        size_t num_strategies);

CDECL void set_strategy_name(instruments_strategy_t strategy, const char * name);

/** Destroy a strategy previously created with
 *  make_strategy or make_redundant_strategy.
 */
CDECL void free_strategy(instruments_strategy_t strategy);

/** Register a set of strategies with the framework and obtain
 *  a handle to an evaluator, which can be used later with
 *  choose_strategy.
 *
 *  strategies contains all interested strategies, singular and redundant.
 *  (see make_strategy and make_redundant_strategy above)
 */
CDECL instruments_strategy_evaluator_t
register_strategy_set(const instruments_strategy_t *strategies, size_t num_strategies);

/** Destroy a strategy evaluator previously created with 
 *  register_strategy_set.
 */
CDECL void free_strategy_evaluator(instruments_strategy_evaluator_t evaluator);

/** Choose and return the best strategy.
 */
CDECL instruments_strategy_t
choose_strategy(instruments_strategy_evaluator_t evaluator, void *chooser_arg);
/* TODO: add 'excludes' for when a strategy is not possible? 
 *       or else the opposite? 
 *       or maybe a 'disable_strategy' function that prevents
 *       choose_strategy from considering a strategy until
 *       'enable_strategy' is called? 
 * NOTE: this only matters if there are more than two 
 *        'singular' strategies. 
 */

/** Choose and return the best nonredundant strategy.
 */
CDECL instruments_strategy_t
choose_nonredundant_strategy(instruments_strategy_evaluator_t evaluator, void *chooser_arg);


typedef void (*instruments_strategy_chosen_callback_t)(instruments_strategy_t, void *);

/** Start an asynchronous call to choose_strategy that will yield
 *  the chosen straegy by calling callback(chosen_strategy).
 *  Returns immediately.
 */
CDECL void
choose_strategy_async(instruments_strategy_evaluator_t evaluator, void *chooser_arg,
                      instruments_strategy_chosen_callback_t callback, void *callback_arg);


/** Queries the evaluator for the recommended time at which
 *  the strategies should be re-evaluated.
 *  This time may be in the past; this indicates that
 *  the strategies should be re-evaluated now.
 */
CDECL struct timeval
get_retry_time(instruments_strategy_evaluator_t evaluator);

/** Save the evaluator's state to a file.
 *  A later call to restore_evaluator will restore this state.
 */
CDECL void
save_evaluator(instruments_strategy_evaluator_t evaluator, const char *filename);

/** Restore an evaluator's state from a file.
 *  Assumes that the evaluator has the same strategies and
 *  estimators as when it was saved.
 */
CDECL void
restore_evaluator(instruments_strategy_evaluator_t evaluator, const char *filename);

/** Opaque handle representing a quantity that the application
 *  uses to calculate the time and cost of its strategies.
 */
typedef void * instruments_estimator_t;

/** Returns an estimator for the downstream bandwidth 
 *  of a network (named by 'iface') in bytes/sec.
 */
//CDECL instruments_estimator_t 
//get_network_bandwidth_down_estimator(const char *iface);

/** Returns an estimator for the upstream bandwidth 
 *  of a network (named by 'iface') in bytes/sec.
 */
//CDECL instruments_estimator_t
//get_network_bandwidth_up_estimator(const char *iface);

/** Returns an estimator for the round-trip time
 *  of a network (named by 'iface').
 */
//CDECL instruments_estimator_t
//get_network_rtt_estimator(const char *iface);

/* ... */

/** Given an estimator (obtained from one of the above functions),
 *  return its value.  Used in eval functions (context is only
 *  available in eval functions).
 */
CDECL double get_estimator_value(instruments_context_t ctx,
                                 instruments_estimator_t estimator);


/* interface for external estimators */

/** Handle for an 'external' estimator - one that takes
 *  values from outside of the Instruments library.
 */
typedef instruments_estimator_t instruments_external_estimator_t;

/** Create an external estimator, initially with no observations. 
 * 
 *  @param name A descriptive name for the quantity that this estimator estimates.
 *              Used for saving and restoring error distributions.
 *              Whitespace will be replaced with '_' characters.
 */
CDECL instruments_external_estimator_t create_external_estimator(const char *name);

/** Destroy an external estimator. */
CDECL void free_external_estimator(instruments_external_estimator_t estimator);

/** Add an observation to an external estimator.
 *
 *  @param observation The latest observation (measurement) of the estimated quantity.
 *  @param new_estimate The value of the estimator just after this observation.
 */
CDECL void add_observation(instruments_external_estimator_t estimator, 
                           double observation, double new_estimate);


/** Set hints for binning estimator values in a histogram.
 *
 *  The framework may decide to store estimator values in a histogram
 *  for the purpose of computing probability-weighted sums.
 *  This gives the framework a hint about reasonable values to use
 *  when choosing the bins (e.g. based on historical measurements).
 *
 * @param 
 */
CDECL void set_estimator_range_hints(instruments_estimator_t estimator,
                                     double min, double max, size_t num_bins);

/** Return 1 iff estimator already has range hints set; else return 0.
 */
CDECL int estimator_has_range_hints(instruments_estimator_t estimator);

typedef enum {
    INSTRUMENTS_DEBUG_LEVEL_NONE=0, /* shut it. */
    INSTRUMENTS_DEBUG_LEVEL_ERROR,  /* exceptional situations only. */
    INSTRUMENTS_DEBUG_LEVEL_INFO,   /* a little chattier, but not in any hot spots. (default) */
    INSTRUMENTS_DEBUG_LEVEL_DEBUG   /* be as verbose and slow as you want. */
} instruments_debug_level_t;

CDECL void instruments_set_debug_level(instruments_debug_level_t level);

#endif
