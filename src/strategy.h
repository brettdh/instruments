#ifndef STRATEGY_H_INCL
#define STRATEGY_H_INCL

#include <vector>
#include <string>
#include <map>
#include <set>
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
    TIME_FN=0, ENERGY_FN, DATA_FN,

    NUM_FNS
};

enum ComparisonType {
    COMPARISON_TYPE_IRRELEVANT,
    SINGULAR_TO_SINGULAR,
    SINGULAR_TO_REDUNDANT
};

class Strategy {
  public:
    Strategy(eval_fn_t time_fn_, 
             eval_fn_t energy_cost_fn_, 
             eval_fn_t data_cost_fn_, 
             void *strategy_arg_,
             void *default_chooser_arg_);
    Strategy(const instruments_strategy_t strategies[], 
             size_t num_strategies, void *default_chooser_arg_=nullptr);

    void setName(const char *name_);
    const char *getName() const;
    
    void addEstimator(typesafe_eval_fn_t fn, Estimator *estimator);
    double calculateTime(StrategyEvaluator *evaluator, void *chooser_arg, ComparisonType comparison_type);
    double calculateCost(StrategyEvaluator *evaluator, void *chooser_arg, ComparisonType comparison_type);
    bool isRedundant();

    double calculateStrategyValue(eval_fn_type_t type, 
                                  StrategyEvaluationContext *ctx, void *chooser_arg);

    void getAllEstimators(StrategyEvaluator *evaluator);
    bool usesEstimator(Estimator *estimator);
    bool usesEstimator(typesafe_eval_fn_t fn, Estimator *estimator);
    bool usesNoEstimators(typesafe_eval_fn_t fn);
    
    std::set<Estimator *> getEstimatorsSet();
    std::vector<Estimator *> getEstimators();

    const std::vector<Strategy *>& getChildStrategies();
    bool includes(Strategy *child);
    bool childrenAreDisjoint(typesafe_eval_fn_t fn);

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

    std::string name;

    typesafe_eval_fn_t fns[NUM_FNS];
    void setEvalFnLookupArray();

    void collectEstimators();

    double expectedValue(StrategyEvaluator *evaluator, typesafe_eval_fn_t fn, void *chooser_arg,
                         ComparisonType comparison_type);


    std::map<typesafe_eval_fn_t, small_set<Estimator*> > estimators;

    std::vector<Strategy *> child_strategies;

  public:
    typesafe_eval_fn_t getEvalFn(eval_fn_type_t type);
};

double redundant_strategy_minimum_time(StrategyEvaluationContext *ctx,
                                       void *strategy_arg, void *chooser_arg);
double redundant_strategy_total_energy_cost(StrategyEvaluationContext *ctx, 
                                            void *strategy_arg, void *chooser_arg);
double redundant_strategy_total_data_cost(StrategyEvaluationContext *ctx, 
                                          void *strategy_arg, void *chooser_arg);

std::string get_value_name(Strategy *strategy, typesafe_eval_fn_t fn);
eval_fn_type_t get_value_type(Strategy *strategy, typesafe_eval_fn_t fn);

#endif
