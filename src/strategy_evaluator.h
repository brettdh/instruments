#ifndef STRATEGY_EVALUATOR_H_INCL
#define STRATEGY_EVALUATOR_H_INCL

#include "instruments.h"
#include "strategy.h"
#include "strategy_evaluation_context.h"
#include "eval_method.h"
#include "thread_pool.h"

#include <vector>
#include <string>

#include <pthread.h>

class Estimator;
class Strategy;

/*
 * These help implement the different uncertainty evaluation methods
 *   by deciding how to return estimator values.
 */

// TODO: should this be a StrategyEvaluationContext subclass?
// TODO:   or should they be merged? I want to be able to get at the
// TODO:   currently-being-evaluated strategy from the context,
// TODO:   so I can add estimators to the strategy as they are being requested.
class StrategyEvaluator : public StrategyEvaluationContext {
  public:
    static StrategyEvaluator *create(const char *name_, 
                                     const instruments_strategy_t *strategies,
                                     size_t num_strategies);
    static StrategyEvaluator *create(const char *name_, 
                                     const instruments_strategy_t *strategies,
                                     size_t num_strategies, EvalMethod type);

    void setSilent(bool silent_);
    bool isSilent();

    void setName(const char *name_);
    const char *getName() const;

    // TODO: declare this not-thread-safe?  that seems reasonable.
    instruments_strategy_t chooseStrategy(void *chooser_arg, bool redundancy=true);
    void chooseStrategyAsync(void *chooser_arg, 
                             instruments_strategy_chosen_callback_t callback,
                             void *callback_arg, bool redundancy=true);
    double getLastStrategyTime(instruments_strategy_t strategy);
    
    instruments_scheduled_reevaluation_t
    scheduleReevaluation(void *chooser_arg, 
                         instruments_pre_evaluation_callback_t pre_evaluation_callback,
                         void *pre_eval_callback_arg,
                         instruments_strategy_chosen_callback_t chosen_callback,
                         void *chosen_callback_arg,
                         double seconds_in_future,
                         bool redundancy=true);
    void cancelReevaluation(instruments_scheduled_reevaluation_t handle);
    
    void addEstimator(Estimator *estimator);
    void removeEstimator(Estimator *estimator);
    bool usesEstimator(Estimator *estimator);

    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg,
                                 ComparisonType comparison_type=COMPARISON_TYPE_IRRELEVANT) = 0;

    // override if the comparison_type argument to expectedValue actually matters.
    virtual bool singularComparisonIsDifferent();

    void observationAdded(Estimator *estimator, double observation, 
                          double old_estimate, double new_estimate);
    void estimatorConditionsChanged(Estimator *estimator);
    
    virtual void saveToFile(const char *filename) = 0;

    // also clears the cache.
    void restoreFromFile(const char *filename);

    virtual ~StrategyEvaluator();
  protected:
    StrategyEvaluator();
    virtual void setStrategies(const instruments_strategy_t *strategies,
                               size_t num_strategies);

    std::vector<Strategy*> strategies;
    const small_set<Estimator*>& getAllEstimators();

    virtual void processObservation(Estimator *estimator, double observation, 
                                    double old_estimate, double new_estimate) { /* ignore by default */ }
    virtual void processEstimatorConditionsChange(Estimator *estimator) { /* ignore by default */ }
    virtual void restoreFromFileImpl(const char *filename) = 0;

    // TODO: change to a better default.
    const static EvalMethod DEFAULT_EVAL_METHOD = TRUSTED_ORACLE;

  private:
    double calculateTime(Strategy *strategy, void *chooser_arg, ComparisonType comparison_type);
    double calculateCost(Strategy *strategy, void *chooser_arg, ComparisonType comparison_type);
    Strategy *currentStrategy;
    bool silent;

    std::string name;

    small_set<Estimator *> subscribed_estimators;

    pthread_mutex_t cache_mutex;
    std::map<void *, instruments_strategy_t> nonredundant_choice_cache;
    std::map<void *, instruments_strategy_t> redundant_choice_cache;
    std::map<instruments_strategy_t, double> strategy_times_cache;

    instruments_strategy_t getCachedChoice(void *chooser_arg, bool redundancy);
    void saveCachedChoice(instruments_strategy_t winner, void *chooser_arg, bool redundancy,
                          std::map<instruments_strategy_t, double>& strategy_times);
    void clearCache();

    
    pthread_mutex_t evaluator_mutex;

    // for asynchronous strategy decisions.
    ThreadPool *pool;
};

struct ScheduledReevaluationHandle {
    ScheduledReevaluationHandle(ThreadPool::TimerTaskPtr task_);
    void cancel();
  private:
    ThreadPool::TimerTaskPtr task;
};

#endif

