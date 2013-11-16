#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <thread>
#include <functional>
using std::thread;
using std::max;
using std::function;

#include <instruments.h>
#include <instruments_private.h>
#include "debug.h"
#include "strategy.h"
#include "estimator.h"
#include "external_estimator.h"
#include "estimator_registry.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"
#include "continuous_distribution.h"

instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return mJ */
              eval_fn_t data_cost_fn, /* return bytes */
              void *strategy_arg, void *default_chooser_arg)
{
    return new Strategy(time_fn, energy_cost_fn, data_cost_fn, 
                        strategy_arg, default_chooser_arg);
}

instruments_strategy_t
make_redundant_strategy(const instruments_strategy_t *strategies, 
                        size_t num_strategies)
{
    return new Strategy(strategies, num_strategies);
}

void 
set_strategy_name(instruments_strategy_t strategy_handle, const char * name)
{
    Strategy *strategy = (Strategy *) strategy_handle;
    strategy->setName(name);
}

const char *
get_strategy_name(instruments_strategy_t strategy_handle)
{
    Strategy *strategy = (Strategy *) strategy_handle;
    return strategy->getName();
}

void free_strategy(instruments_strategy_t strategy)
{
    delete ((Strategy *) strategy);
}

double get_adjusted_estimator_value(instruments_context_t ctx, Estimator *estimator)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext *>(ctx);
    if (context) {
        return context->getAdjustedEstimatorValue(estimator);
    } else {
        // app just wants the raw value.
        ASSERT(estimator);
        return estimator->getEstimate();
    }
}

double get_estimator_value(instruments_context_t ctx,
                           instruments_estimator_t est_handle)
{
    Estimator *estimator = static_cast<Estimator*>(est_handle);
    return get_adjusted_estimator_value(ctx, estimator);
}


instruments_estimator_t
get_network_bandwidth_down_estimator(const char *iface)
{
    return EstimatorRegistry::getNetworkBandwidthDownEstimator(iface);
}

instruments_estimator_t
get_network_bandwidth_up_estimator(const char *iface)
{
    return EstimatorRegistry::getNetworkBandwidthUpEstimator(iface);
}

instruments_estimator_t
get_network_rtt_estimator(const char *iface)
{
    return EstimatorRegistry::getNetworkRttEstimator(iface);
}

instruments_strategy_evaluator_t
register_strategy_set(const char *name, const instruments_strategy_t *strategies, size_t num_strategies)
{
    return StrategyEvaluator::create(name, strategies, num_strategies);
}

instruments_strategy_evaluator_t
register_strategy_set_with_cmp(const char *name, const instruments_strategy_t *strategies, size_t num_strategies,
                               struct instruments_chooser_arg_fns chooser_arg_fns)
{
    return StrategyEvaluator::create(name, strategies, num_strategies, chooser_arg_fns);
}

instruments_strategy_evaluator_t
register_strategy_set_with_method(const char *name, const instruments_strategy_t *strategies, size_t num_strategies,
                                  EvalMethod method)
{
    return StrategyEvaluator::create(name, strategies, num_strategies, method);
}

instruments_strategy_evaluator_t
register_strategy_set_with_method_and_fns(const char *name, const instruments_strategy_t *strategies, size_t num_strategies,
                                          EvalMethod method, struct instruments_chooser_arg_fns chooser_arg_fns)
{
    return StrategyEvaluator::create(name, strategies, num_strategies, method, chooser_arg_fns);
}

void
free_strategy_evaluator(instruments_strategy_evaluator_t e)
{
    StrategyEvaluator *evaluator = static_cast<StrategyEvaluator*>(e);
    delete evaluator;
}

void set_strategy_evaluator_name(instruments_strategy_evaluator_t e, const char *name)
{
    StrategyEvaluator *evaluator = static_cast<StrategyEvaluator*>(e);
    evaluator->setName(name);
}

const char *
get_strategy_evaluator_name(instruments_strategy_evaluator_t e)
{
    StrategyEvaluator *evaluator = static_cast<StrategyEvaluator*>(e);
    return evaluator->getName();
}

instruments_strategy_t
choose_strategy(instruments_strategy_evaluator_t evaluator_handle,
                void *chooser_arg)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->chooseStrategy(chooser_arg);
}

instruments_strategy_t
choose_nonredundant_strategy(instruments_strategy_evaluator_t evaluator_handle,
                             void *chooser_arg)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->chooseStrategy(chooser_arg, /* redundancy = */ false);
}

double
get_last_strategy_time(instruments_strategy_evaluator_t evaluator_handle,
                       instruments_strategy_t strategy)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->getLastStrategyTime(strategy);
}

/* Functions for use in composing one strategy from another. */

/** Given an eval context and a strategy, returns the strategy's completion time. */
double
calculate_strategy_time(instruments_context_t ctx, instruments_strategy_t strategy_handle, 
                        void *chooser_arg)
{
    Strategy *strategy = (Strategy *) strategy_handle;
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext *>(ctx);
    return strategy->calculateStrategyValue(TIME_FN, context, chooser_arg);
}

/** Given an eval context and a strategy, returns the strategy's energy cost. */
double
calculate_strategy_energy(instruments_context_t ctx, instruments_strategy_t strategy_handle, 
                          void *chooser_arg)
{
    Strategy *strategy = (Strategy *) strategy_handle;
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext *>(ctx);
    return strategy->calculateStrategyValue(ENERGY_FN, context, chooser_arg);
}

/** Given an eval context and a strategy, returns the strategy's cellular data cost. */
double
calculate_strategy_data(instruments_context_t ctx, instruments_strategy_t strategy_handle, 
                        void *chooser_arg)
{
    Strategy *strategy = (Strategy *) strategy_handle;
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext *>(ctx);
    return strategy->calculateStrategyValue(DATA_FN, context, chooser_arg);
}


void
choose_strategy_async(instruments_strategy_evaluator_t evaluator_handle,
                      void *chooser_arg,
                      instruments_strategy_chosen_callback_t callback,
                      void *callback_arg)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    evaluator->chooseStrategyAsync(chooser_arg, callback, callback_arg);
}

void
choose_nonredundant_strategy_async(instruments_strategy_evaluator_t evaluator_handle,
                                   void *chooser_arg,
                                   instruments_strategy_chosen_callback_t callback,
                                   void *callback_arg)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    evaluator->chooseStrategyAsync(chooser_arg, callback, callback_arg, /* redundancy = */ false);
}


instruments_scheduled_reevaluation_t
schedule_reevaluation(instruments_strategy_evaluator_t evaluator_handle,
                      void *chooser_arg,
                      instruments_pre_evaluation_callback_t pre_evaluation_callback,
                      void *pre_eval_callback_arg,
                      instruments_strategy_chosen_callback_t chosen_callback,
                      void *chosen_callback_arg,
                      double seconds_in_future)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->scheduleReevaluation(chooser_arg, 
                                           pre_evaluation_callback, pre_eval_callback_arg,
                                           chosen_callback, chosen_callback_arg, 
                                           seconds_in_future);
}

instruments_scheduled_reevaluation_t
schedule_nonredundant_reevaluation(instruments_strategy_evaluator_t evaluator_handle,
                                   void *chooser_arg,
                                   instruments_pre_evaluation_callback_t pre_evaluation_callback,
                                   void *pre_eval_callback_arg,
                                   instruments_strategy_chosen_callback_t chosen_callback,
                                   void *chosen_callback_arg,
                                   double seconds_in_future)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->scheduleReevaluation(chooser_arg, 
                                           pre_evaluation_callback, pre_eval_callback_arg,
                                           chosen_callback, chosen_callback_arg, 
                                           seconds_in_future, /* redundancy = */ false);
}

void cancel_scheduled_reevaluation(instruments_scheduled_reevaluation_t handle)
{
    auto real_handle = static_cast<ScheduledReevaluationHandle *>(handle);
    real_handle->cancel();
}

void free_scheduled_reevaluation(instruments_scheduled_reevaluation_t handle)
{
    auto real_handle = static_cast<ScheduledReevaluationHandle *>(handle);
    delete real_handle;
}

void
save_evaluator(instruments_strategy_evaluator_t evaluator_handle, const char *filename)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    evaluator->saveToFile(filename);
}

void
restore_evaluator(instruments_strategy_evaluator_t evaluator_handle, const char *filename)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    evaluator->restoreFromFile(filename);
}


static instruments_estimator_t
get_coin_flip_heads_estimator()
{
    return EstimatorRegistry::getCoinFlipEstimator();
}

int coin_flip_lands_heads(instruments_context_t ctx)
{
    instruments_estimator_t estimator = get_coin_flip_heads_estimator();
    double value = get_estimator_value(ctx, estimator);
    
    return (value >= 0.5);
}

void reset_coin_flip_estimator(EstimatorType type)
{
    EstimatorRegistry::resetEstimator("CoinFlip", type);
}

void add_coin_flip_observation(int heads)
{
    EstimatorRegistry::getCoinFlipEstimator()->addObservation(heads ? 1.0 : 0.0);
}

instruments_external_estimator_t
create_external_estimator(const char *name)
{
    return new ExternalEstimator(name);
}

void free_external_estimator(instruments_external_estimator_t est_handle)
{
    Estimator *estimator = static_cast<Estimator *>(est_handle);
    delete estimator;
}

void add_observation(instruments_external_estimator_t est_handle, 
                     double observation, double new_estimate)
{
    ExternalEstimator *estimator = static_cast<ExternalEstimator *>(est_handle);
    estimator->addObservation(observation, new_estimate);
}


void set_estimator_range_hints(instruments_estimator_t est_handle,
                               double min, double max, size_t num_bins)
{
    Estimator *estimator = static_cast<Estimator *>(est_handle);
    estimator->setRangeHints(min, max, num_bins);
}

int estimator_has_range_hints(instruments_estimator_t est_handle)
{
    Estimator *estimator = static_cast<Estimator *>(est_handle);
    return estimator->hasRangeHints() ? 1 : 0;
}

void set_estimator_condition(instruments_estimator_t est_handle,
                             instruments_estimator_condition_type_t condition_type,
                             double value)
{
    Estimator *estimator = static_cast<Estimator *>(est_handle);
    ConditionType type = ConditionType(condition_type);
    estimator->setCondition(type, value);
}

void clear_estimator_conditions(instruments_estimator_t est_handle)
{
    Estimator *estimator = static_cast<Estimator *>(est_handle);
    estimator->clearConditions();
}

static function<bool(instruments_strategy_t)>
get_bound_finder(instruments_estimator_condition_type_t bound_type,
                 instruments_strategy_t current_winner)
{
    if (bound_type == INSTRUMENTS_ESTIMATOR_VALUE_AT_LEAST) {
        return [=](instruments_strategy_t winner) { return (winner != current_winner); };
    } else {
        return [=](instruments_strategy_t winner) { return (winner == current_winner); };
    }
}

static double find_upper_bound(instruments_strategy_evaluator_t evaluator,
                               instruments_estimator_t estimator, 
                               instruments_estimator_condition_type_t bound_type, 
                               decltype(choose_strategy) chooser, 
                               double lower, 
                               instruments_strategy_t current_winner,
                               void *chooser_arg)
{
    // O(lg(N)) search for rough upper bound on tipping point
    // N is at most DBL_MAX; in practice, it will be much less
    
    instruments_strategy_t winner = current_winner;
    set_estimator_condition(estimator, bound_type, lower);
    winner = chooser(evaluator, chooser_arg);
    function<bool(instruments_strategy_t)> found_bound = get_bound_finder(bound_type, current_winner);
    while (!found_bound(winner)) {
        lower = max(lower * 2.0, 1.0);
        clear_estimator_conditions(estimator);
        set_estimator_condition(estimator, bound_type, lower);
        winner = chooser(evaluator, chooser_arg);
    }
    clear_estimator_conditions(estimator);
    return lower;
}


struct EstimatorBound 
calculate_tipping_point(instruments_strategy_evaluator_t evaluator, 
                        instruments_estimator_t estimator,
                        instruments_estimator_condition_type_t bound_type,
                        int redundant, 
                        instruments_strategy_t current_winner,
                        void *chooser_arg)
{
    // StrategyEvaluator *evaluator = static_cast<StrategyEvaluator*>(evaluator_handle);
    // Estimator *estimator = static_cast<Estimator*>(estimator_handle);
    // Strategy *current_winner = static_cast<Strategy*>(current_winner_handle);
    // Strategy *future_winner = static_cast<Strategy*>(future_winner_handle);
    // return evaluator->calculateTippingPoint(estimator, bound_type, current_winner, future_winner);

    /* First, assume that the evaluator decision is monotonic as a function of a bound on one estimator,
     * (with all others evaluated over their error distributions).  In other words, if wifi beats 3G
     * and I increase the latency bound on wifi, eventually it will be worse than 3G, but it will
     * never become better again as a result of further latency increase.  Hence, there is one
     * tipping point rather than many.
     *
     * Next, since I can assume that, I can binary search over a range of possible bound values
     * to find the tipping point.  If the bound is an upper bound, the range is [0.0, current_value],
     * since I'm assuming estimator values are >= 0.  If the bound is a lower bound, the range is
     * [current_value, N), where N is theoretically infinite.  In order to make the search finite, 
     * I will calculate N by starting with current_value and doubling it until the decision changes.
     * I can then proceed with binary search.  If N overflows, I assume that the bound won't work,
     * and I will discover this in lg(DBL_MAX) steps.  For normal usage (i.e. the bound works),
     * I will discover the limits much faster.
     */
    
    //double estimate = get_estimator_value(NULL, estimator);

    /*
    double lowest_value, highest_value;
    if (estimator_has_range_hints(estimator)) {
        Estimator *est = static_cast<Estimator *>(estimator);
        EstimatorRangeHints hints = est->getRangeHints();

        // use the range hints to set some loose bounds.  
        //  hopefully the hints are reasonably good and these work.
        static const double hedge_factor = 3.0;
        lowest_value = hints.min / hedge_factor;
        highest_value = hints.max * hedge_factor;
    } else {
        lowest_value = 0.0;
        highest_value = DBL_MAX;
    }
    */
    
    auto chooser = (redundant ? choose_strategy : choose_nonredundant_strategy);

    double lower, upper;
    bool is_lower_bound;
    if (bound_type == INSTRUMENTS_ESTIMATOR_VALUE_AT_LEAST) {
        is_lower_bound = true;
    } else {
        assert(bound_type == INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST);
        is_lower_bound = false;
    } 
    lower = 0.0;
    upper = find_upper_bound(evaluator, estimator, bound_type, chooser, 0.0, current_winner, chooser_arg);
    
    // XXX: not sure whether the starting range is correct.  
    // must be a range where one endpoint results in current_winner
    //  and the other endpoint results in a different strategy.
    // NOTE: if there is more than one strategy change (e.g. wifi -> redundant -> 3G),
    //        this will return the tipping point of the FIRST strategy change, 
    //        which is exactly what we want.
    

#ifndef NDEBUG
    clear_estimator_conditions(estimator);
    set_estimator_condition(estimator, bound_type, lower);
    instruments_strategy_t lower_winner = chooser(evaluator, chooser_arg);
    clear_estimator_conditions(estimator);
    
    set_estimator_condition(estimator, bound_type, upper);
    instruments_strategy_t upper_winner = chooser(evaluator, chooser_arg);
    clear_estimator_conditions(estimator);

    if (is_lower_bound) {
        assert(lower_winner == current_winner);
        assert(upper_winner != current_winner);
    } else {
        assert(lower_winner != current_winner);
        assert(upper_winner == current_winner);
    }
#endif

    double threshold = 0.1; // XXX: maybe too coarse or fine; TODO: time this code, see how long it takes
    EstimatorBound bound;
    while (upper - lower > threshold) {
        clear_estimator_conditions(estimator);
        double mid = (upper + lower) / 2.0;
        set_estimator_condition(estimator, bound_type, mid);

        instruments_strategy_t winner = chooser(evaluator, chooser_arg);
        if (winner == current_winner) {
            if (is_lower_bound) { // if lower bound,
                lower = mid;      // then tipping point is above the midpoint
                bound.value = upper;
            } else {
                upper = mid;      // else tipping point is below the midpoint
                bound.value = lower;
            }
        } else {
            if (is_lower_bound) { // if lower bound,
                upper = mid;      // then tipping point is below the midpoint
                bound.value = upper;
            } else {
                lower = mid;      // else tipping point is above the midpoint
                bound.value = lower;
            }
        }
    }
    clear_estimator_conditions(estimator);
    bound.valid = true; //TODO: detect failure case
    
    return bound;
}


instruments_continuous_distribution_t create_continuous_distribution(double shape, double scale)
{
    return new ContinuousDistribution(shape, scale);
}

void free_continuous_distribution(instruments_continuous_distribution_t distribution_handle)
{
    ContinuousDistribution *dist = (ContinuousDistribution *) distribution_handle;
    delete dist;
}

double get_probability_value_is_in_range(instruments_continuous_distribution_t distribution_handle,
                                         double lower, double upper)
{
    ContinuousDistribution *dist = (ContinuousDistribution *) distribution_handle;
    return dist->getProbabilityValueIsInRange(lower, upper);
}
