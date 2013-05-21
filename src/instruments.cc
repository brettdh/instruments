#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

#include <instruments.h>
#include <instruments_private.h>
#include "strategy.h"
#include "estimator.h"
#include "external_estimator.h"
#include "estimator_registry.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"

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

void free_strategy(instruments_strategy_t strategy)
{
    delete ((Strategy *) strategy);
}

double get_adjusted_estimator_value(instruments_context_t ctx, Estimator *estimator)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext *>(ctx);
    return context->getAdjustedEstimatorValue(estimator);
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


struct timeval
get_retry_time(instruments_strategy_evaluator_t evaluator_handle)
{
    // TODO: implement for real.
    struct timeval now;
    gettimeofday(&now, NULL);
    return now;
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
