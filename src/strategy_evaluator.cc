#include <assert.h>
#include "strategy_evaluator.h"
#include "trusted_oracle_strategy_evaluator.h"
#include "empirical_error_strategy_evaluator.h"
#include "confidence_bounds_strategy_evaluator.h"
#include "bayesian_strategy_evaluator.h"
#include "strategy.h"
#include "estimator.h"

#include "pthread_util.h"

#include "small_set.h"

#include "debug.h"
namespace inst = instruments;
using inst::INFO;

#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <signal.h>

#include <vector>
using std::vector;

StrategyEvaluator::StrategyEvaluator()
    : currentStrategy(NULL), silent(false)
{
    pthread_mutex_init(&evaluator_mutex, NULL);
    
    const int ASYNC_EVAL_THREADS = 3;
    pool = new ThreadPool(ASYNC_EVAL_THREADS);
}

StrategyEvaluator::~StrategyEvaluator()
{
    for (Estimator *estimator : subscribed_estimators) {
        estimator->unsubscribe(this);
    }
    delete pool;
}

void
StrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                 size_t num_strategies)
{
    strategies.clear();
    
    for (size_t i = 0; i < num_strategies; ++i) {
        Strategy *strategy = (Strategy *)new_strategies[i];
        strategy->getAllEstimators(this); // subscribes this to all estimators
        strategies.push_back(strategy);
    }
}

void
StrategyEvaluator::addEstimator(Estimator *estimator)
{
    estimator->subscribe(this);
    subscribed_estimators.insert(estimator);
}

const small_set<Estimator*>&
StrategyEvaluator::getAllEstimators()
{
    return subscribed_estimators;
}

void
StrategyEvaluator::removeEstimator(Estimator *estimator)
{
    // estimator is letting me know it's going away; don't call unsubscribe
    subscribed_estimators.erase(estimator);
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
    } else if (type == CONFIDENCE_BOUNDS) {
        evaluator = new ConfidenceBoundsStrategyEvaluator;
    } else if (type == BAYESIAN) {
        evaluator = new BayesianStrategyEvaluator;
    } else {
        // TODO: implement the rest.
        assert(false);
        __builtin_unreachable();
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

void
StrategyEvaluator::setSilent(bool silent_)
{
    silent = silent_;
}

bool 
StrategyEvaluator::isSilent()
{
    return silent;
}

instruments_strategy_t
StrategyEvaluator::chooseStrategy(void *chooser_arg, bool redundancy)
{
    PthreadScopedLock lock(&evaluator_mutex);
    
    assert(currentStrategy == NULL);

    // first, pick the singular strategy that takes the least time (expected)
    Strategy *best_singular = NULL;
    double best_singular_time = 0.0;

    // not the "best cost," but the cost of the best (min-time) singular strategy.
    double best_singular_cost = 0.0;
    for (vector<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (!currentStrategy->isRedundant()) {
            inst::dbgprintf(INFO, "Evaluating singular strategy \"%s\"\n",
                            currentStrategy->getName());
            inst::dbgprintf(INFO, "Calculating time\n");
            double time = calculateTime(currentStrategy, chooser_arg);

            double cost = 0.0;
            if (redundancy) {
                inst::dbgprintf(INFO, "Calculating cost\n");
                // calculate the singular-strategy cost so I don't have to do it
                //  later when calculating redundant-strategy costs.
                // XXX: HACK.  This is a caching decision that belongs inside
                // XXX:  the class that does the caching.
                cost = calculateCost(currentStrategy, chooser_arg);
            }

            inst::dbgprintf(INFO, "Singular strategy \"%s\"  time: %f  cost: %f\n",
                            currentStrategy->getName(), time, cost);

            if (!best_singular || time < best_singular_time) {
                best_singular = currentStrategy;
                best_singular_time = time;
                best_singular_cost = cost;
            }
        }
    }
    
    if (!redundancy) {
        inst::dbgprintf(INFO, "Not considering redundancy; returning best "
                        "singular strategy (time %f cost %f)\n",
                        best_singular_time, best_singular_cost);
        return best_singular;
    }

    // then, pick the cheapest redundant strategy that offers net benefit
    //  over the best singular strategy (if any)
    Strategy *best_redundant = NULL;
    double best_redundant_net_benefit = 0.0;
    for (vector<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (currentStrategy->isRedundant()) {
            inst::dbgprintf(INFO, "Evaluating redundant strategy \"%s\"\n",
                            currentStrategy->getName());
            double redundant_time = calculateTime(currentStrategy, chooser_arg);
            double benefit = best_singular_time - redundant_time;

            double redundant_cost = calculateCost(currentStrategy, chooser_arg);
            double extra_redundant_cost = redundant_cost - best_singular_cost;
            double net_benefit = benefit - extra_redundant_cost;

            if (!silent) {
                inst::dbgprintf(INFO, "Best singular strategy time: %f\n", best_singular_time);
                inst::dbgprintf(INFO, "Redundant strategy time: %f\n", redundant_time);
                inst::dbgprintf(INFO, "Redundant strategy benefit: %f\n", benefit);
                inst::dbgprintf(INFO, "Best-time singular strategy cost: %f\n", best_singular_cost);
                inst::dbgprintf(INFO, "Redundant strategy cost: %f\n", redundant_cost);
                inst::dbgprintf(INFO, "Redundant strategy additional cost: %f\n", 
                                extra_redundant_cost);
            }
            if (currentStrategy->includes(best_singular)) {
                // because the redundant strategy includes the best singular strategy,
                //  the singular strategy can never have a lower time, and
                //  the redundant strategy can never have a lower cost.
                // if either happens, the strategy evaluator is broken.
                assert(benefit >= -0.0001); // tolerate floating-point inaccuracy
                assert(extra_redundant_cost >= -0.0001);
            }

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


void
StrategyEvaluator::chooseStrategyAsync(void *chooser_arg, 
                                       instruments_strategy_chosen_callback_t callback,
                                       void *callback_arg)
{
    auto async_choose = [=]() {
        instruments_strategy_t strategy = chooseStrategy(chooser_arg);
        callback(strategy, callback_arg); // thread-safety of this is up to the caller
    };

    pool->startTask(async_choose);
}
