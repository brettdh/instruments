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
 *     Returns the estimated energy expenditure in milliJoules.
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
              eval_fn_t energy_cost_fn, /* return milliJoules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *strategy_arg,
              void *default_chooser_arg);

/** Create a *redundant* strategy by combining two or more
 *  single-option strategies.
 */
CDECL instruments_strategy_t
make_redundant_strategy(const instruments_strategy_t *strategies, 
                        size_t num_strategies, void *default_chooser_arg);

CDECL void set_strategy_name(instruments_strategy_t strategy, const char * name);
CDECL const char *get_strategy_name(instruments_strategy_t strategy);

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
register_strategy_set(const char *name, const instruments_strategy_t *strategies, size_t num_strategies);

/** Destroy a strategy evaluator previously created with 
 *  register_strategy_set.
 */
CDECL void free_strategy_evaluator(instruments_strategy_evaluator_t evaluator);

struct instruments_chooser_arg_fns {
    /* comparator function type for chooser_arg passed to choose_strategy.
     * returns  1 if first argument is less
     *          0 if first argument is greater-or-equal
     */
    int (*chooser_arg_less)(void *, void *);

    /* functions to allow the framework to create and destroy its own
     * internal copies of the chooser arg, for caching purposes. 
     */
    void * (*copy_chooser_arg)(void *);
    void (*delete_chooser_arg)(void *);
};

CDECL instruments_strategy_evaluator_t
register_strategy_set_with_fns(const char *name, const instruments_strategy_t *strategies, size_t num_strategies,
                               struct instruments_chooser_arg_fns chooser_arg_fns);


CDECL void set_strategy_evaluator_name(instruments_strategy_evaluator_t evaluator, const char * name);
CDECL const char *get_strategy_evaluator_name(instruments_strategy_evaluator_t evaluator);

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

/** Choose and return the best nonredundant strategy, considering only performance and ignoring costs.
 */
CDECL instruments_strategy_t
choose_nonredundant_strategy_ignore_cost(instruments_strategy_evaluator_t evaluator, void *chooser_arg);

/** Returns the calculated completion time for the given strategy. 
 *
 *  This does not begin a new computation; it merely returns
 *  the last computed completion time for the given strategy.
 *  It should therefore only be called after a call to choose_strategy.
 */
CDECL double
get_last_strategy_time(instruments_strategy_evaluator_t evaluator, 
                       instruments_strategy_t strategy);

/* Functions for use in composing one strategy from another. */

/** Given an eval context and a strategy, returns the strategy's completion time. */
CDECL double
calculate_strategy_time(instruments_context_t ctx, instruments_strategy_t strategy, 
                        void *chooser_arg);

/** Given an eval context and a strategy, returns the strategy's energy cost. */
CDECL double
calculate_strategy_energy(instruments_context_t ctx, instruments_strategy_t strategy, 
                          void *chooser_arg);

/** Given an eval context and a strategy, returns the strategy's cellular data cost. */
CDECL double
calculate_strategy_data(instruments_context_t ctx, instruments_strategy_t strategy, 
                        void *chooser_arg);

typedef void (*instruments_strategy_chosen_callback_t)(instruments_strategy_t, void *);

/** Start an asynchronous call to choose_strategy that will yield
 *  the chosen straegy by calling callback(chosen_strategy).
 *  Returns immediately.
 */
CDECL void
choose_strategy_async(instruments_strategy_evaluator_t evaluator, void *chooser_arg,
                      instruments_strategy_chosen_callback_t callback, void *callback_arg);

CDECL void
choose_nonredundant_strategy_async(instruments_strategy_evaluator_t evaluator, void *chooser_arg,
                                   instruments_strategy_chosen_callback_t callback, void *callback_arg);


typedef void * instruments_scheduled_reevaluation_t;
typedef void (*instruments_pre_evaluation_callback_t)(void *);

/** Schedule an asynchronous re-evaluation the specified time in the future.
 * 
 *  This is equivalent to choose_strategy_async, with two simple differences:
 *  1) The evaluation happens in the future, as opposed to immediately.
 *  2) Before the evaluation, pre_evaluation_callback is called with the provided argument.
 *     This gives the application a chance to update its estimators
 *     (especially to set conditions, in order to use conditional probabilities
 *      when doing re-evaluation) before the evaluation is done.
 *
 *  The return value is a handle that allows the application to cancel a re-evaluation.
 *  This handle should be destroyed with free_scheduled_reevaluation() when
 *  the application has no further use for it.
 */
CDECL instruments_scheduled_reevaluation_t
schedule_reevaluation(instruments_strategy_evaluator_t evaluator_handle,
                      void *chooser_arg,
                      instruments_pre_evaluation_callback_t pre_evaluation_callback,
                      void *pre_eval_callback_arg,
                      instruments_strategy_chosen_callback_t chosen_callback,
                      void *chosen_callback_arg,
                      double seconds_in_future);

CDECL instruments_scheduled_reevaluation_t
schedule_nonredundant_reevaluation(instruments_strategy_evaluator_t evaluator_handle,
                                   void *chooser_arg,
                                   instruments_pre_evaluation_callback_t pre_evaluation_callback,
                                   void *pre_eval_callback_arg,
                                   instruments_strategy_chosen_callback_t chosen_callback,
                                   void *chosen_callback_arg,
                                   double seconds_in_future);

/** Cancel the scheduled re-evaluation.
 *
 *  If the re-evaluation has not already run, it will not run in the future.
 *  The handle still must be freed with free_scheduled_reevaluation().
 */
CDECL void 
cancel_scheduled_reevaluation(instruments_scheduled_reevaluation_t handle);

/** Frees the memory associated with the scheduled re-evaluation.
 *
 *  Note that this does NOT cancel the reevaluation, and that
 *  it is (obviously) invalid to do so after freeing it.
 */ 
CDECL void 
free_scheduled_reevaluation(instruments_scheduled_reevaluation_t handle);

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
    INSTRUMENTS_ESTIMATOR_VALUE_AT_LEAST,
    INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST,
} instruments_estimator_condition_type_t;

/** Set a condition on the range of this estimator, 
 *  most likely because of a known-good current observation.
 *  For instance, if the estimator tracks wifi session length,
 *  and it's known that the current session is at least 30 seconds long,
 *  setting a condition of AT_LEAST 30 will tell instruments to
 *  take that into account in its evaluation.
 *
 *  If the given condition is already set on this estimator, 
 *  the new bound value replaces the old one.
 */
CDECL void set_estimator_condition(instruments_estimator_t estimator, 
                                   instruments_estimator_condition_type_t condition_type,
                                   double value);

/** Clear all conditions on this estimator set by set_estimator_condition.
 */
CDECL void clear_estimator_conditions(instruments_estimator_t estimator);

/** Reset error for this estimator -- either to no error or historical error values,
 *  depending on whether the evaluators that use this estimator have previously
 *  restored error from a history file.
 */
CDECL void reset_estimator_error(instruments_estimator_t estimator_handle);

#include "estimator_bound.h"

/** For a given estimator, calculate the conditional bound of the given type
 *  at which point the estimator's bounded distribution will cause the evaluator
 *  to switch from current_winner to future_winner.
 *  Returns an EstimatorBound struct, where
 *    bound.valid = true iff the estimator found such a bound
 *    bound.value = the bound, iff bound.valid
 */
CDECL struct EstimatorBound calculate_tipping_point(instruments_strategy_evaluator_t evaluator, 
                                                    instruments_estimator_t estimator,
                                                    instruments_estimator_condition_type_t bound_type, 
                                                    int redundant, 
                                                    instruments_strategy_t current_winner,
                                                    void *chooser_arg);
                                             

typedef void * instruments_continuous_distribution_t;

/** Create a Weibull distribution with the given parameters.
 */
CDECL instruments_continuous_distribution_t create_continuous_distribution(double shape, double scale);

/** Frees the given distribution object.
 */
CDECL void free_continuous_distribution(instruments_continuous_distribution_t distribution);

/** Returns the probability that a sample from the distribution 
 *  is in [lower, upper] given that it is >= lower.
 *  Note that this is NOT Pr(X >= lower ^ X < upper); 
 *  it is instead Pr(X < upper | X >= lower).
 */
CDECL double get_probability_value_is_in_range(instruments_continuous_distribution_t distribution,
                                               double lower, double upper);

typedef enum {
    INSTRUMENTS_DEBUG_LEVEL_NONE=0, /* shut it. */
    INSTRUMENTS_DEBUG_LEVEL_ERROR,  /* exceptional situations only. */
    INSTRUMENTS_DEBUG_LEVEL_INFO,   /* a little chattier, but not in any hot spots. (default) */
    INSTRUMENTS_DEBUG_LEVEL_DEBUG   /* be as verbose and slow as you want. */
} instruments_debug_level_t;

CDECL void instruments_set_debug_level(instruments_debug_level_t level);

#endif
