#include <stdlib.h>

#include <instruments.h>
#include <instruments_private.h>


instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *arg)
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
    EstimatorSet::ExpectationEvaluator *evaluator = 
        (EstimatorSet::ExpectationEvaluator *) ctx;
    Estimator *estimator = EstimatorRegistry::getNetworkBandwidthDownEstimator(iface);
    return evaluator->getAdjustedEstimatorValue(estimator);
}

double network_bandwidth_up(instruments_context_t ctx, const char *iface)
{
    EstimatorSet::ExpectationEvaluator *evaluator = 
        (EstimatorSet::ExpectationEvaluator *) ctx;
    Estimator *estimator = EstimatorRegistry::getNetworkBandwidthUpEstimator(iface);
    return evaluator->getAdjustedEstimatorValue(estimator);
}

double network_rtt(instruments_context_t ctx, const char *iface)
{
    EstimatorSet::ExpectationEvaluator *evaluator = 
        (EstimatorSet::ExpectationEvaluator *) ctx;
    Estimator *estimator = EstimatorRegistry::getNetworkRttEstimator(iface);
    return evaluator->getAdjustedEstimatorValue(estimator);
}


ssize_t
choose_strategy(const instruments_strategy_t *strategies, size_t num_strategies)
{
    // first, pick the singular strategy that takes the least time (expected)
    Strategy *best_singular = NULL;
    double best_singular_time = 0.0;
    for (size_t i = 0; i <= num_strategies; ++i) {
        Strategy *cur_strategy = strategies[i];
        if (!cur_strategy->isRedundant()) {
            double time = cur_strategy->calculateTime();
            if (!best_singular || time < best_singular_time) {
                best_singular = cur_strategy;
                best_singular_time = time;
            }
        }
    }
    
    // then, pick the cheapest redundant strategy that offers net benefit
    //  over the best singular strategy (if any)
    Strategy *best_redundant = NULL;
    double best_redundant_net_benefit = 0.0;
    for (size_t i = 0; i <= num_strategies; ++i) {
        Strategy *cur_strategy = strategies[i];
        if (cur_strategy->isRedundant()) {
            double benefit = best_singular_time - cur_strategy->calculateTime();
            double net_benefit = benefit - (cur_strategy->calculateCost() -
                                            best_singular->calculateCost());
            if (net_benefit > 0.0 && 
                (best_redundant == NULL || net_benefit > best_redundant_net_benefit)) {
                best_redundant = cur_strategy;
                best_redundant_net_benefit = net_benefit;
            }
        }
    }

    // if any redundant strategy was better than the best singular strategy, use it.
    //  otherwise, just use the best singular strategy.
    if (best_redundant) {
        return best_redundant;
    } else {
        return best_singular;
    }
}

void add_fair_coin_estimator(instruments_strategy_t strategy)
{
    /* TODO */
}

void add_heads_heavy_coin_estimator(instruments_strategy_t strategy)
{
    /* TODO */
}

int fair_coin_lands_heads(instruments_context_t ctx)
{
    /* TODO */
    return 0;
}

int heads_heavy_coin_lands_heads(instruments_context_t ctx)
{
    /* TODO */
    return 0;
}
