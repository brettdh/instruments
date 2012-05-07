#ifndef ESTIMATOR_SET_H_INCL
#define ESTIMATOR_SET_H_INCL

#include <set>
#include "instruments.h"
#include "eval_method.h"

class Estimator;
class Strategy;

/*
 * These help implement the different uncertainty evaluation methods
 *   by deciding how to return estimator values.
 */
class StrategyEvaluator {
  public:
    static StrategyEvaluator *create(const instruments_strategy_t *strategies,
                                     size_t num_strategies);
    static StrategyEvaluator *create(const instruments_strategy_t *strategies,
                                     size_t num_strategies, EvalMethod type);
    instruments_strategy_t chooseStrategy();
    
    virtual double expectedValue(eval_fn_t fn, void *arg);
    virtual void observationAdded(Estimator *estimator, double value) = 0;
    
    virtual void startIteration() = 0;
    virtual void finishIteration() = 0;
    virtual double jointProbability() = 0;
    virtual double getAdjustedEstimatorValue(Estimator *estimator) = 0;
    virtual void advance() = 0;

    bool isDone();
  protected:
    StrategyEvaluator();
    void setStrategies(const instruments_strategy_t *strategies,
                       size_t num_strategies);

    bool done;
    std::set<Strategy*> strategies;
    std::set<Estimator*> estimators;

    // TODO: change to a better default.
    const static EvalMethod DEFAULT_EVAL_METHOD = TRUSTED_ORACLE;

  private:
    double calculateTime(Strategy *strategy);
    double calculateCost(Strategy *strategy);
};

#endif

