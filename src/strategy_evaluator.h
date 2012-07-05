#ifndef STRATEGY_EVALUATOR_H_INCL
#define STRATEGY_EVALUATOR_H_INCL

#include <set>
#include "instruments.h"
#include "strategy.h"
#include "strategy_evaluation_context.h"
#include "eval_method.h"

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

    // TODO: declare this not-thread-safe?  that seems reasonable.
    instruments_strategy_t chooseStrategy();
    
    void addEstimator(Estimator *estimator);
    bool usesEstimator(Estimator *estimator);

    virtual double expectedValue(typesafe_eval_fn_t fn, void *arg) = 0;
    virtual void observationAdded(Estimator *estimator, double value) = 0;

    virtual instruments_estimator_t getEstimatorContext(Estimator *estimator);

  protected:
    StrategyEvaluator();
    void setStrategies(const instruments_strategy_t *strategies,
                       size_t num_strategies);

    std::set<Strategy*> strategies;
    std::set<Estimator*> estimators;

    // TODO: change to a better default.
    const static EvalMethod DEFAULT_EVAL_METHOD = TRUSTED_ORACLE;

  private:
    double calculateTime(Strategy *strategy);
    double calculateCost(Strategy *strategy);
    Strategy *currentStrategy;
};

#endif

