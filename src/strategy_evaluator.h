#ifndef STRATEGY_EVALUATOR_H_INCL
#define STRATEGY_EVALUATOR_H_INCL

#include "instruments.h"
#include "strategy.h"
#include "strategy_evaluation_context.h"
#include "eval_method.h"
#include "thread_pool.h"

#include <vector>

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
    static StrategyEvaluator *create(const instruments_strategy_t *strategies,
                                     size_t num_strategies);
    static StrategyEvaluator *create(const instruments_strategy_t *strategies,
                                     size_t num_strategies, EvalMethod type);

    void setSilent(bool silent_);
    bool isSilent();

    // TODO: declare this not-thread-safe?  that seems reasonable.
    instruments_strategy_t chooseStrategy(void *chooser_arg, bool redundancy=true);
    void chooseStrategyAsync(void *chooser_arg, 
                             instruments_strategy_chosen_callback_t callback);
    
    void addEstimator(Estimator *estimator);
    void removeEstimator(Estimator *estimator);
    bool usesEstimator(Estimator *estimator);

    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg) = 0;
    virtual void observationAdded(Estimator *estimator, double observation, 
                                  double old_estimate, double new_estimate) { /* ignore by default */ }

    virtual void saveToFile(const char *filename) = 0;
    virtual void restoreFromFile(const char *filename) = 0;

    virtual ~StrategyEvaluator();
  protected:
    StrategyEvaluator();
    virtual void setStrategies(const instruments_strategy_t *strategies,
                               size_t num_strategies);

    std::vector<Strategy*> strategies;
    const small_set<Estimator*>& getAllEstimators();

    // TODO: change to a better default.
    const static EvalMethod DEFAULT_EVAL_METHOD = TRUSTED_ORACLE;

  private:
    double calculateTime(Strategy *strategy, void *chooser_arg);
    double calculateCost(Strategy *strategy, void *chooser_arg);
    Strategy *currentStrategy;
    bool silent;

    small_set<Estimator *> subscribed_estimators;

    // for asynchronous strategy decisions.
    ThreadPool *pool;
};

#endif

