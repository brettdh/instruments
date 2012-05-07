#include <set>
#include <assert.h>
#include "strategy_evaluator.h"
#include "trusted_oracle_strategy_evaluator.h"
#include "strategy.h"

using std::set;

StrategyEvaluator::StrategyEvaluator()
    : done(false)
{
}

void
StrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                 size_t num_strategies)
{
    strategies.clear();
    estimators.clear();
    
    for (size_t i = 0; i < num_strategies; ++i) {
        Strategy *strategy = (Strategy *)new_strategies[i];
        for (set<Estimator*>::const_iterator it = strategy->estimators.begin();
             it != strategy->estimators.end(); ++it) {
            estimators.insert(*it);
        }
        strategies.insert(strategy);
    }
}

StrategyEvaluator *
StrategyEvaluator::create(const instruments_strategy_t *strategies,
                          size_t num_strategies)
{
    return create(strategies, num_strategies, DEFAULT_EVAL_METHOD);
}

StrategyEvaluator *
StrategyEvaluator::create(const instruments_strategy_t *strategies,
                          size_t num_strategies, EvalMethod type)
{
    StrategyEvaluator *evaluator = NULL;
    switch (type) {
    case TRUSTED_ORACLE:
        evaluator = new TrustedOracleStrategyEvaluator;
        break;
    default:
        // TODO: implement the rest.
        assert(0);
    }
    evaluator->setStrategies(strategies, num_strategies);
    return evaluator;
}


double
StrategyEvaluator::expectedValue(eval_fn_t fn, void *arg)
{
    double weightedSum = 0.0;

    startIteration();
    while (!isDone()) {
        double prob = jointProbability();
        double value = fn(this, arg);
        weightedSum += (prob * value);
        
        advance();
    }
    finishIteration();
    return weightedSum;
}

bool
StrategyEvaluator::isDone()
{
    return done;
}

double
StrategyEvaluator::calculateTime(Strategy *strategy)
{
    return expectedValue(strategy->time_fn, strategy->fn_arg);
}

double
StrategyEvaluator::calculateCost(Strategy *strategy)
{
    return expectedValue(strategy->data_cost_fn, strategy->fn_arg);
}


instruments_strategy_t
StrategyEvaluator::chooseStrategy()
{
    // first, pick the singular strategy that takes the least time (expected)
    Strategy *best_singular = NULL;
    double best_singular_time = 0.0;
    for (set<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        Strategy *cur_strategy = *it;
        if (!cur_strategy->isRedundant()) {
            double time = calculateTime(cur_strategy);
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
    for (set<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        Strategy *cur_strategy = *it;
        if (cur_strategy->isRedundant()) {
            double benefit = best_singular_time - calculateTime(cur_strategy);
            double net_benefit = benefit - (calculateCost(cur_strategy) -
                                            calculateCost(best_singular));
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
