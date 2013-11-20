#include <assert.h>
#include <math.h>
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
#include <map>
using std::vector; using std::map;

StrategyEvaluator::DelegatingChooserArgComparator::
DelegatingChooserArgComparator(StrategyEvaluator *evaluator_)
    : evaluator(evaluator_) 
{
}

bool 
StrategyEvaluator::DelegatingChooserArgComparator::
operator()(void *left, void *right)
{
    return evaluator->chooser_arg_fns.chooser_arg_less(left, right);
};

static int default_chooser_arg_less(void *left, void *right)
{
    return (left < right);
}

static void *default_copy_chooser_arg(void *arg)
{
    return arg;
}

static void default_delete_chooser_arg(void *arg)
{
    // no-op
}

struct instruments_chooser_arg_fns default_chooser_arg_fns = {
    default_chooser_arg_less,
    default_copy_chooser_arg,
    default_delete_chooser_arg
};

StrategyEvaluator::StrategyEvaluator(bool trivial)
    : currentStrategy(NULL), silent(false), subscribe_all(!trivial), delegating_comparator(this),
      chooser_arg_fns(default_chooser_arg_fns),
      nonredundant_choice_cache(delegating_comparator),
      redundant_choice_cache(delegating_comparator)
{
    MY_PTHREAD_MUTEX_INIT(&evaluator_mutex);
    MY_PTHREAD_MUTEX_INIT(&cache_mutex);
    
    const int ASYNC_EVAL_THREADS = 3;
    if (trivial) {
        pool = nullptr;
    } else {
        pool = new ThreadPool(ASYNC_EVAL_THREADS);
    }
}

StrategyEvaluator::~StrategyEvaluator()
{
    for (Estimator *estimator : subscribed_estimators) {
        estimator->unsubscribe(this);
    }
    delete pool;
    clearCache();
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
    if (subscribe_all) {
        estimator->subscribe(this);
        subscribed_estimators.insert(estimator);
    }
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
StrategyEvaluator::create(const char *name_, 
                          const instruments_strategy_t *strategies,
                          size_t num_strategies,
                          struct instruments_chooser_arg_fns chooser_arg_fns)
{
    return create(name_, strategies, num_strategies, DEFAULT_EVAL_METHOD, chooser_arg_fns);
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
StrategyEvaluator::create(const char *name_, 
                          const instruments_strategy_t *strategies,
                          size_t num_strategies, EvalMethod type,
                          struct instruments_chooser_arg_fns chooser_arg_fns_)
{
    //wait_for_debugger();

    StrategyEvaluator *evaluator = NULL;
    if (type == TRUSTED_ORACLE) {
        evaluator = new TrustedOracleStrategyEvaluator;
    } else if (type & EMPIRICAL_ERROR) {
        evaluator = new EmpiricalErrorStrategyEvaluator(type);
    } else if (type == CONFIDENCE_BOUNDS) {
        evaluator = new ConfidenceBoundsStrategyEvaluator(false);
    } else if (type == CONFIDENCE_BOUNDS_WEIGHTED) {
        evaluator = new ConfidenceBoundsStrategyEvaluator(true);
    } else if (type == BAYESIAN) {
        evaluator = new BayesianStrategyEvaluator(false);
    } else if (type == BAYESIAN_WEIGHTED) {
        evaluator = new BayesianStrategyEvaluator(true);
    } else {
        // TODO: implement the rest.
        ASSERT(false);
        __builtin_unreachable();
    }
    evaluator->setName(name_);
    evaluator->setStrategies(strategies, num_strategies);
    evaluator->chooser_arg_fns = chooser_arg_fns_;
    return evaluator;
}

double
StrategyEvaluator::calculateTime(Strategy *strategy, void *chooser_arg, ComparisonType comparison_type)
{
    return strategy->calculateTime(this, chooser_arg, comparison_type);
}

double
StrategyEvaluator::calculateCost(Strategy *strategy, void *chooser_arg, ComparisonType comparison_type)
{
    return strategy->calculateCost(this, chooser_arg, comparison_type);
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
StrategyEvaluator::getCachedChoice(void *chooser_arg, bool redundancy)
{
    PthreadScopedLock lock(&cache_mutex);
    auto& cache = (redundancy ? redundant_choice_cache : nonredundant_choice_cache);

    auto it = cache.find(chooser_arg);
    if (it != cache.end()) {
        return it->second;
    }
    return NULL;
}

void
StrategyEvaluator::saveCachedChoice(instruments_strategy_t winner, void *chooser_arg, bool redundancy,
                                    map<instruments_strategy_t, double>& strategy_times,
                                    map<instruments_strategy_t, double>& strategy_costs)
{
    PthreadScopedLock lock(&cache_mutex);
    auto& cache = (redundancy ? redundant_choice_cache : nonredundant_choice_cache);

    auto it = cache.find(chooser_arg);
    if (it != cache.end()) {
        it->second = winner;
    } else {
        void *my_copy = chooser_arg_fns.copy_chooser_arg(chooser_arg);
        cache[my_copy] = winner;
    }
    
    ASSERT(strategy_times.size() == strategy_costs.size());
    for (auto& p : strategy_times) {
        instruments_strategy_t strategy = p.first;
        double strategy_time = p.second;
        ASSERT(strategy_costs.count(strategy) > 0);
        double strategy_cost = strategy_costs[strategy];
        
        strategy_times_cache[strategy] = strategy_time;
        strategy_costs_cache[strategy] = strategy_cost;
    }
}

void
StrategyEvaluator::clearCache()
{
    PthreadScopedLock lock(&cache_mutex);
    for (auto &cache : {nonredundant_choice_cache, redundant_choice_cache}) {
        for (auto& it : cache) {
            void *my_copy = it.first;
            chooser_arg_fns.delete_chooser_arg(my_copy);
        }
    }
    nonredundant_choice_cache.clear();
    redundant_choice_cache.clear();
    
    strategy_times_cache.clear();
    strategy_costs_cache.clear();
}

void
StrategyEvaluator::observationAdded(Estimator *estimator, double observation, 
                                    double old_estimate, double new_estimate)
{
    clearCache();

    PthreadScopedLock lock(&evaluator_mutex);
    processObservation(estimator, observation, old_estimate, new_estimate);
}

void
StrategyEvaluator::estimatorConditionsChanged(Estimator *estimator)
{
    clearCache();

    PthreadScopedLock lock(&evaluator_mutex);
    processEstimatorConditionsChange(estimator);
}

instruments_strategy_t
StrategyEvaluator::chooseStrategy(void *chooser_arg, bool redundancy)
{
    instruments_strategy_t cached_choice = getCachedChoice(chooser_arg, redundancy);
    if (cached_choice) {
        return cached_choice;
    }

    PthreadScopedLock lock(&evaluator_mutex);
    
    ASSERT(currentStrategy == NULL);

    map<instruments_strategy_t, double> strategy_times;
    map<instruments_strategy_t, double> strategy_costs;

    // turn this on and off.  if false, uses time as ranking for singular strategies.
    // if true, uses weighted cost function.
    // redundancy eval always uses weighted cost function.
    const bool OPTIMIZE_WEIGHTED_COST_FUNCTION = true;

    // first, pick the singular strategy that takes the least time (expected)
    // WHOOPS!  I should be picking the singular strategy that minimizes the weighted cost function.
    Strategy *best_singular = NULL;
    double best_singular_time = 0.0;

    // not the "best cost," but the cost of the best singular strategy.
    double best_singular_cost = 0.0;
    for (vector<Strategy *>::const_iterator it = strategies.begin();
         it != strategies.end(); ++it) {
        currentStrategy = *it;
        if (!currentStrategy->isRedundant()) {
            inst::dbgprintf(INFO, "Evaluating singular strategy \"%s\"\n",
                            currentStrategy->getName());
            inst::dbgprintf(INFO, "Calculating time\n");
            double time = calculateTime(currentStrategy, chooser_arg, SINGULAR_TO_SINGULAR);
            ASSERT(!isnan(time));
            strategy_times[currentStrategy] = time;

            double cost = 0.0;
            bool new_winner = (best_singular == NULL);

            if (OPTIMIZE_WEIGHTED_COST_FUNCTION) {
                inst::dbgprintf(INFO, "Calculating cost\n");
                // calculate the singular-strategy cost so I don't have to do it
                //  later when calculating redundant-strategy costs.
                // XXX: HACK.  This is a caching decision that belongs inside
                // XXX:  the class that does the caching.
                cost = calculateCost(currentStrategy, chooser_arg, SINGULAR_TO_SINGULAR);
                ASSERT(!isnan(cost));
                strategy_costs[currentStrategy] = cost;
                inst::dbgprintf(INFO, "Singular strategy \"%s\"  time: %f  cost: %f  sum: %f\n",
                                currentStrategy->getName(), time, cost, time + cost);

                new_winner = new_winner || ((time + cost) < (best_singular_time + best_singular_cost));
            } else {
                inst::dbgprintf(INFO, "Singular strategy \"%s\"  time: %f\n",
                                currentStrategy->getName(), time);
                new_winner = new_winner || (time < best_singular_time);
            }

            if (new_winner) {
                best_singular = currentStrategy;
                best_singular_time = time;
                best_singular_cost = cost;
            }
        }
    }
    currentStrategy = NULL;
    
    if (!redundancy) {
        inst::dbgprintf(INFO, "Not considering redundancy; returning best "
                        "singular strategy (time %f)\n",
                        best_singular_time);
        saveCachedChoice(best_singular, chooser_arg, redundancy, strategy_times, strategy_costs);
        return best_singular;
    }

    if (singularComparisonIsDifferent()) {
        // recalculate time and cost
        best_singular_time = calculateTime(best_singular, chooser_arg, SINGULAR_TO_REDUNDANT);
        best_singular_cost = calculateCost(best_singular, chooser_arg, SINGULAR_TO_REDUNDANT);
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
            double redundant_time = calculateTime(currentStrategy, chooser_arg,
                                                  SINGULAR_TO_REDUNDANT);
            ASSERT(!isnan(redundant_time));
            strategy_times[currentStrategy] = redundant_time;
            double benefit = best_singular_time - redundant_time;

            double redundant_cost = calculateCost(currentStrategy, chooser_arg,
                                                  SINGULAR_TO_REDUNDANT);
            ASSERT(!isnan(redundant_cost));
            strategy_costs[currentStrategy] = redundant_cost;
            
            double extra_redundant_cost = redundant_cost - best_singular_cost;
            double net_benefit = benefit - extra_redundant_cost;

            /*
              NOTE: this calculation is exactly the same as the comparison
                    between singular strategies above, just written in the
                    language of why you might choose a redundant strategy.
                    
              net_benefit = benefit - extra_redundant_cost

              benefit - extra_redundant_cost > 0.0  # condition for redundancy winning
              benefit > extra_redundant_cost
              best_singular_time - redundant_time > redundant_cost - best_singular_cost
              best_singular_time + best_singular_cost > redundant_time + redundant_cost
             */

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
                ASSERT(benefit >= -0.0001); // tolerate floating-point inaccuracy
                ASSERT(extra_redundant_cost >= -0.0001);
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
    instruments_strategy_t winner = NULL;
    if (best_redundant) {
        winner = best_redundant;
    } else {
        winner = best_singular;
    }
    
    saveCachedChoice(winner, chooser_arg, redundancy, strategy_times, strategy_costs);
    return winner;
}

double
StrategyEvaluator::getLastStrategyTime(instruments_strategy_t strategy)
{
    PthreadScopedLock lock(&cache_mutex);
    if (strategy_times_cache.count(strategy) > 0) {
        return strategy_times_cache[strategy];
    }
    return 0.0;
}

bool
StrategyEvaluator::strategyGapIsWidening(Strategy *current_winner, bool redundant,
                                         map<Strategy*, double>& last_strategy_badness)
{
    /*
      I want to know whether the given strategy is getting better or worse
      relative to the other strategies, based only on the changing conditional
      bound on an estimator.  

      This will only be called when trying to find the upper bound of 
      a range where the winning strategy is different at the endpoints.
      If the winner at the lower bound is getting better as I increase 
      the upper bound, I will stop trying to find an upper bound, because 
      I know it's infinite.

      Let t = time step; i.e. evaluation count.
      Define badness(strategy, t) = time(t) + cost(t)

      This strategy has gotten better if:
      - For each other strategy "s":
          badness(s, t) - badness(winner, t) < badness(s, t-1) - badness(winner, t-1)
            or equivalently (and slightly more understandably):
          badness(s, t) - badness(s, t-1) < badness(winner, t) - badness(winner, t-1)
      Otherwise, it has gotten worse with respect to one of them.
      
      Written another way: if this is true for the min-badness strategy, 
      it is true for all of them.

      Remember, I'm assuming that a strategy doesn't go from winning to losing back to winning
      just by increasing the bound on one estimator, because the badness is a monotonic
      function of that estimator.

      I think I'm also assuming that this function's second derivative is zero... hmm. XXX
      Welp, that assumption holds for latency and utterance size, so.
     */
    

    PthreadScopedLock lock(&cache_mutex);

    double min_badness_gap = 0.0;
    Strategy *min_gap_strategy = nullptr;
    for (Strategy *strategy : strategies) {
        if (strategy == current_winner || 
            (!redundant && strategy->isRedundant())) {
            continue;
        }

        // it must be there, because we just called choose_strategy, and we held
        // the evaluator mutex beforehand (and still hold it).
        ASSERT(strategy_times_cache.count(strategy) == 1);
        ASSERT(strategy_costs_cache.count(strategy) == 1);
        double strategy_time = strategy_times_cache[strategy];
        double strategy_cost = strategy_costs_cache[strategy];
        double strategy_badness = strategy_time + strategy_cost;

        if (last_strategy_badness.count(strategy) > 0) {
            double last_badness = last_strategy_badness[strategy];
            double last_badness_gap = strategy_badness - last_badness;
            if (!min_gap_strategy || last_badness_gap < min_badness_gap) {
                min_badness_gap = last_badness_gap;
                min_gap_strategy = strategy;
            }
        }
        
        last_strategy_badness[strategy] = strategy_badness;
    }

    ASSERT(strategy_times_cache.count(current_winner) == 1);
    ASSERT(strategy_costs_cache.count(current_winner) == 1);
    
    double cur_strategy_time = strategy_times_cache[current_winner];
    double cur_strategy_cost = strategy_costs_cache[current_winner];
    double cur_strategy_badness = cur_strategy_time + cur_strategy_cost;
    bool widening = false;
    if (min_gap_strategy) {
        double cur_strategy_gap = cur_strategy_badness - last_strategy_badness[current_winner];
        widening = (cur_strategy_gap < min_badness_gap);
    }
    last_strategy_badness[current_winner] = cur_strategy_badness;
    return widening;
}


void
StrategyEvaluator::chooseStrategyAsync(void *chooser_arg, 
                                       instruments_strategy_chosen_callback_t callback,
                                       void *callback_arg, bool redundancy)
{
    // because choose_strategy_async returns immediately, 
    // we need to make sure the chooser_arg still exists when 
    // chooseStrategy is actually called.
    void *my_copy = chooser_arg_fns.copy_chooser_arg(chooser_arg);
    
    auto async_choose = [=]() {
        instruments_strategy_t strategy = chooseStrategy(my_copy, redundancy);
        chooser_arg_fns.delete_chooser_arg(my_copy);
        callback(strategy, callback_arg); // thread-safety of this is up to the caller
    };

    pool->startTask(async_choose);
}

instruments_scheduled_reevaluation_t
StrategyEvaluator::scheduleReevaluation(void *chooser_arg, 
                                        instruments_pre_evaluation_callback_t pre_evaluation_callback,
                                        void *pre_eval_callback_arg,
                                        instruments_strategy_chosen_callback_t chosen_callback,
                                        void *chosen_callback_arg,
                                        double seconds_in_future,
                                        bool redundancy)
{
    // same here; we even need a copy before we call async.
    // that creates two copies here, but that's still cheap compared
    // to everything else that's going on anyway.
    void *my_copy = chooser_arg_fns.copy_chooser_arg(chooser_arg);
    auto reevaluate = [=]() {
        pre_evaluation_callback(pre_eval_callback_arg);
        chooseStrategyAsync(my_copy, chosen_callback, chosen_callback_arg, redundancy);
        chooser_arg_fns.delete_chooser_arg(my_copy);
    };
    ThreadPool::TimerTaskPtr task = pool->scheduleTask(seconds_in_future, reevaluate);
    return new ScheduledReevaluationHandle(task);
}

void
StrategyEvaluator::cancelReevaluation(instruments_scheduled_reevaluation_t handle)
{
    ScheduledReevaluationHandle *wrapper = (ScheduledReevaluationHandle *) handle;
    wrapper->cancel();
}

void 
StrategyEvaluator::restoreFromFile(const char *filename)
{
    clearCache();
    restoreFromFileImpl(filename);
}

ScheduledReevaluationHandle::ScheduledReevaluationHandle(ThreadPool::TimerTaskPtr task_)
    : task(task_)
{
    // implicit destructor takes care of freeing shared ptr
}

void 
ScheduledReevaluationHandle::cancel()
{
    task->cancel();
}

bool
StrategyEvaluator::singularComparisonIsDifferent()
{
    // by default.  override in subclass if it actually matters.
    return false;
}

void
StrategyEvaluator::setName(const char *name_)
{
    name = name_;
}

const char *
StrategyEvaluator::getName() const
{
    return name.c_str();
}
