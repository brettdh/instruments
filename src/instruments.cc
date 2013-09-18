#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <thread>
using std::thread;

#include <instruments.h>
#include <instruments_private.h>
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
register_strategy_set(const instruments_strategy_t *strategies, size_t num_strategies)
{
    return StrategyEvaluator::create(strategies, num_strategies);
}

instruments_strategy_evaluator_t
register_strategy_set_with_method(const instruments_strategy_t *strategies, size_t num_strategies,
                                  EvalMethod method)
{
    return StrategyEvaluator::create(strategies, num_strategies, method);
}

void
free_strategy_evaluator(instruments_strategy_evaluator_t e)
{
    StrategyEvaluator *evaluator = static_cast<StrategyEvaluator*>(e);
    delete evaluator;
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
