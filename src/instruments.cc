#include <stdlib.h>

#include <instruments.h>
#include <instruments_private.h>
#include "strategy.h"
#include "redundant_strategy.h"
#include "estimator.h"
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
    return new RedundantStrategy(strategies, num_strategies);
}

void free_strategy(instruments_strategy_t strategy)
{
    delete ((Strategy *) strategy);
}


void add_network_bandwidth_down_estimator(instruments_strategy_t strategy,
                                          const char *iface)
{
    /* TODO */
}

void add_network_bandwidth_up_estimator(instruments_strategy_t strategy,
                                        const char *iface)
{
    /* TODO */
}

void add_network_rtt_estimator(instruments_strategy_t strategy,
                               const char *iface)
{
    /* TODO */
}


double network_bandwidth_down(instruments_context_t ctx, const char *iface)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    Estimator *estimator = EstimatorRegistry::getNetworkBandwidthDownEstimator(iface);
    return context->getAdjustedEstimatorValue(estimator);
}

double network_bandwidth_up(instruments_context_t ctx, const char *iface)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    Estimator *estimator = EstimatorRegistry::getNetworkBandwidthUpEstimator(iface);
    return context->getAdjustedEstimatorValue(estimator);
}

double network_rtt(instruments_context_t ctx, const char *iface)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    Estimator *estimator = EstimatorRegistry::getNetworkRttEstimator(iface);
    return context->getAdjustedEstimatorValue(estimator);
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
free_strategy_evaluator(instruments_strategy_evaluator_t evaluator)
{
    
}

instruments_strategy_t
choose_strategy(instruments_strategy_evaluator_t evaluator_handle)
{
    StrategyEvaluator *evaluator = (StrategyEvaluator *) evaluator_handle;
    return evaluator->chooseStrategy();
}

void add_coin_flip_estimator(instruments_strategy_t s)
{
    Strategy *strategy = (Strategy *) s;
    strategy->addEstimator(EstimatorRegistry::getCoinFlipEstimator());
}

int coin_flip_lands_heads(instruments_context_t ctx)
{
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    Estimator *estimator = EstimatorRegistry::getCoinFlipEstimator();
    double value = context->getAdjustedEstimatorValue(estimator);
    
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
    StrategyEvaluationContext *context = static_cast<StrategyEvaluationContext*>(ctx);
    return context->getAdjustedEstimatorValue(estimator);
}
