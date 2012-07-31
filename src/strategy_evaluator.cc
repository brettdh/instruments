#include <set>
#include <assert.h>
#include "strategy_evaluator.h"
#include "trusted_oracle_strategy_evaluator.h"
#include "empirical_error_strategy_evaluator.h"
#include "strategy.h"
#include "estimator.h"

using std::set;

StrategyEvaluator::StrategyEvaluator()
    : currentStrategy(NULL)
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
        strategy->getAllEstimators(this);
        strategies.insert(strategy);
    }
}

void
StrategyEvaluator::addEstimator(Estimator *estimator)
{
    estimators.insert(estimator);
    estimator->subscribe(this);
}

bool
StrategyEvaluator::usesEstimator(Estimator *estimator)
{
    return (estimators.count(estimator) != 0);
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
    case EMPIRICAL_ERROR:
        evaluator = new EmpiricalErrorStrategyEvaluator;
        break;
    default:
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
    for (set<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (!currentStrategy->isRedundant()) {
            double time = calculateTime(currentStrategy, chooser_arg);
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
    for (set<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (currentStrategy->isRedundant()) {
            double benefit = best_singular_time - calculateTime(currentStrategy, chooser_arg);
            double net_benefit = benefit - (calculateCost(currentStrategy, chooser_arg) -
                                            calculateCost(best_singular, chooser_arg));
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

instruments_estimator_t 
StrategyEvaluator::getEstimatorContext(Estimator *estimator)
{
    if (currentStrategy) {
        currentStrategy->addEstimator(estimator);
        this->addEstimator(estimator);
    }
    
    return this->StrategyEvaluationContext::getEstimatorContext(estimator);
}
