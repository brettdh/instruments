#ifndef STRATEGY_H_INCL
#define STRATEGY_H_INCL

#include <vector>
#include "instruments.h"
#include "small_set.h"

class Estimator;
class EstimatorSet;
class StrategyEvaluator;
class StrategyEvaluationContext;

// use this internally to avoid nasty vtable issues caused by
//  casting to void* from a pointer to a polymorphic subtype.
// (We don't care about the type of arg; it's from the application,
//  which must manage its own type casting.)
typedef double (*typesafe_eval_fn_t)(StrategyEvaluationContext *, void *strategy_arg, void *chooser_arg);

enum eval_fn_type_t {
    TIME_FN, ENERGY_FN, DATA_FN
};

class Strategy {
  public:
    Strategy(eval_fn_t time_fn_, 
             eval_fn_t energy_cost_fn_, 
             eval_fn_t data_cost_fn_, 
             void *strategy_arg_,
             void *default_chooser_arg_);
    Strategy(const instruments_strategy_t strategies[], 
             size_t num_strategies);
    
    void addEstimator(Estimator *estimator);
    double calculateTime(StrategyEvaluator *evaluator, void *chooser_arg);
    double calculateCost(StrategyEvaluator *evaluator, void *chooser_arg);
    bool isRedundant();

    void getAllEstimators(StrategyEvaluator *evaluator);
    bool usesEstimator(Estimator *estimator);

    const std::vector<Strategy *>& getChildStrategies();
    bool childrenAreDisjoint();
    typesafe_eval_fn_t getEvalFn(eval_fn_type_t type);
  private:
    friend class EmpiricalErrorStrategyEvaluatorTest;
    friend class MultiStrategyJointErrorIterator;

    friend double redundant_strategy_minimum_time(StrategyEvaluationContext *ctx,
                                                  void *strategy_arg, void *chooser_arg);
    friend double redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, 
                                                       void *strategy_arg, void *chooser_arg);
    friend double redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, 
                                                     void *strategy_arg, void *chooser_arg);

    typesafe_eval_fn_t time_fn;
    typesafe_eval_fn_t energy_cost_fn;
    typesafe_eval_fn_t data_cost_fn;
    void *strategy_arg;
    void *default_chooser_arg;

    void collectEstimators();

    small_set<Estimator*> estimators;

    std::vector<Strategy *> child_strategies;
};

double redundant_strategy_minimum_time(StrategyEvaluationContext *ctx,
                                       void *strategy_arg, void *chooser_arg);
double redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, 
                                            void *strategy_arg, void *chooser_arg);
double redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, 
                                          void *strategy_arg, void *chooser_arg);

#endif
