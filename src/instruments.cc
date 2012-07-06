#include <stdlib.h>
#include <sys/time.h>

#include <instruments.h>
#include <instruments_private.h>
#include "strategy.h"
#include "estimator.h"
#include "external_estimator.h"
#include "estimator_context.h"
#include "estimator_registry.h"
#include "strategy_evaluator.h"
#include "strategy_evaluation_context.h"

instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *fn_arg)
{
    return new Strategy(time_fn, energy_cost_fn, data_cost_fn, fn_arg);
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

double get_estimator_value(instruments_estimator_t handle)
{
    EstimatorContext *estimatorCtx = static_cast<EstimatorContext*>(handle);
    Estimator *estimator = estimatorCtx->estimator;
    StrategyEvaluationContext *context = estimatorCtx->context;
    return context->getAdjustedEstimatorValue(estimator);
}

static instruments_estimator_t
get_estimator_context(instruments_context_t ctx, Estimator *estimator)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    return context->getEstimatorContext(estimator);
}

instruments_estimator_t
get_network_bandwidth_down_estimator(instruments_context_t ctx, const char *iface)
{
    return get_estimator_context(ctx, EstimatorRegistry::getNetworkBandwidthDownEstimator(iface));
}

instruments_estimator_t
get_network_bandwidth_up_estimator(instruments_context_t ctx, const char *iface)
{
    return get_estimator_context(ctx, EstimatorRegistry::getNetworkBandwidthUpEstimator(iface));
}

instruments_estimator_t
get_network_rtt_estimator(instruments_context_t ctx, const char *iface)
{
    return get_estimator_context(ctx, EstimatorRegistry::getNetworkRttEstimator(iface));
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
choose_strategy(instruments_strategy_evaluator_t evaluator_handle)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->chooseStrategy();
}

struct timeval
get_retry_time(instruments_strategy_evaluator_t evaluator_handle)
{
    // TODO: implement for real.
    struct timeval now;
    gettimeofday(&now, NULL);
    return now;
}

instruments_estimator_t
get_coin_flip_heads_estimator(instruments_context_t ctx)
{
    return get_estimator_context(ctx, EstimatorRegistry::getCoinFlipEstimator());
}

int coin_flip_lands_heads(instruments_context_t ctx)
{
    instruments_estimator_t estimator = get_coin_flip_heads_estimator(ctx);
    double value = get_estimator_value(estimator);
    
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

double get_adjusted_estimator_value(instruments_context_t ctx, Estimator *estimator)
{
    instruments_estimator_t estimatorContext = get_estimator_context(ctx, estimator);
    return get_estimator_value(estimatorContext);
}


instruments_external_estimator_t
create_external_estimator()
{
    return new ExternalEstimator();
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

double get_external_estimator_value(instruments_context_t ctx,
                                    instruments_external_estimator_t est_handle)
{
    Estimator *estimator = static_cast<Estimator*>(est_handle);
    instruments_estimator_t estimatorContext = get_estimator_context(ctx, estimator);
    return get_estimator_value(estimatorContext);
}
