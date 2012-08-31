#include <assert.h>
#include "strategy_evaluator.h"
#include "trusted_oracle_strategy_evaluator.h"
#include "empirical_error_strategy_evaluator.h"
#include "strategy.h"
#include "estimator.h"

#include "small_set.h"

#include <stdio.h>
#include <sys/select.h>
#include <signal.h>

#include <vector>
using std::vector;

StrategyEvaluator::StrategyEvaluator()
    : currentStrategy(NULL)
{
}

void
StrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                 size_t num_strategies)
{
    strategies.clear();
    //estimators.clear();
    
    for (size_t i = 0; i < num_strategies; ++i) {
        Strategy *strategy = (Strategy *)new_strategies[i];
        strategy->getAllEstimators(this);
        strategies.push_back(strategy);
    }
}

void
StrategyEvaluator::addEstimator(Estimator *estimator)
{
    //estimators.insert(estimator);
    estimator->subscribe(this);
}

bool
StrategyEvaluator::usesEstimator(Estimator *estimator)
{
    for (vector<Strategy*>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        if ((*it)->usesEstimator(estimator)) {
            return true;
        }
    }
    return false;
}

StrategyEvaluator *
StrategyEvaluator::create(const instruments_strategy_t *strategies,
                          size_t num_strategies)
{
    return create(strategies, num_strategies, DEFAULT_EVAL_METHOD);
}

void wait_for_debugger()
{
    fprintf(stderr, "ATTACH DEBUGGER NOW!\n");
    int flag = 1;
    while (flag) {
        struct timeval dur = {1, 0};
        (void)select(0, NULL, NULL, NULL, &dur);
    }
}

StrategyEvaluator *
StrategyEvaluator::create(const instruments_strategy_t *strategies,
                          size_t num_strategies, EvalMethod type)
{
    //wait_for_debugger();

    StrategyEvaluator *evaluator = NULL;
    if (type == TRUSTED_ORACLE) {
        evaluator = new TrustedOracleStrategyEvaluator;
    } else if (type & EMPIRICAL_ERROR) {
        evaluator = new EmpiricalErrorStrategyEvaluator(type);
    } else {
        // TODO: implement the rest.
        assert(0);
    }
    evaluator->setStrategies(strategies, num_strategies);
    return evaluator;
}

double
StrategyEvaluator::calculateTime(Strategy *strategy, void *chooser_arg)
{
    return strategy->calculateTime(this, chooser_arg);
}

double
StrategyEvaluator::calculateCost(Strategy *strategy, void *chooser_arg)
{
    return strategy->calculateCost(this, chooser_arg);
}


instruments_strategy_t
StrategyEvaluator::chooseStrategy(void *chooser_arg)
{
    // TODO: error message about concurrent calls
    assert(currentStrategy == NULL);

    // first, pick the singular strategy that takes the least time (expected)
    Strategy *best_singular = NULL;
    double best_singular_time = 0.0;
    for (vector<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (!currentStrategy->isRedundant()) {
            double time = calculateTime(currentStrategy, chooser_arg);

            // calculate the singular-strategy cost so I don't have to do it
            //  later when calculating redundant-strategy costs.
            // XXX: HACK.  This is a caching decision that belongs inside
            // XXX:  the class that does the caching.
            double cost = calculateCost(currentStrategy, chooser_arg);
            (void)cost;

            
            if (!best_singular || time < best_singular_time) {
                best_singular = currentStrategy;
                best_singular_time = time;
            }
        }
    }
    
    // then, pick the cheapest redundant strategy that offers net benefit
    //  over the best singular strategy (if any)
    Strategy *best_redundant = NULL;
    double best_redundant_net_benefit = 0.0;
    for (vector<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (currentStrategy->isRedundant()) {
            double redundant_time = calculateTime(currentStrategy, chooser_arg);
            double benefit = best_singular_time - redundant_time;
            double singular_cost = calculateCost(best_singular, chooser_arg);
            double redundant_cost = calculateCost(currentStrategy, chooser_arg);
            double net_benefit = benefit - (redundant_cost - singular_cost);
            //fprintf(stderr, "Best singular strategy time: %f\n", best_singular_time);
            //fprintf(stderr, "Redundant strategy time: %f\n", redundant_time);
            //fprintf(stderr, "Redundant strategy benefit: %f\n", benefit);
            //fprintf(stderr, "Redundant strategy additional cost: %f\n", redundant_cost - singular_cost);
            if (net_benefit > 0.0 && 
                (best_redundant == NULL || net_benefit > best_redundant_net_benefit)) {
                best_redundant = currentStrategy;
                best_redundant_net_benefit = net_benefit;
            }
        }
    }
    currentStrategy = NULL;

    // if any redundant strategy was better than the best singular strategy, use it.
    //  otherwise, just use the best singular strategy.
    if (best_redundant) {
        return best_redundant;
    } else {
        return best_singular;
    }
}
