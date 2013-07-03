#ifndef CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL
#define CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

#include <string>
#include <vector>
#include <map>

class ConfidenceBoundsStrategyEvaluator : public StrategyEvaluator {
  public:
    ConfidenceBoundsStrategyEvaluator(bool weighted_);
    virtual ~ConfidenceBoundsStrategyEvaluator();
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);
    virtual double getAdjustedEstimatorValue(Estimator *estimator);

    virtual void saveToFile(const char *filename);
    virtual void restoreFromFileImpl(const char *filename);
  protected:
    virtual void processObservation(Estimator *estimator, double observation,
                                    double old_estimate, double new_estimate);
  private:
    enum BoundType {
        LOWER=0, UPPER
    };

    enum EvalMode {
        CONSERVATIVE=0, // lower-bound benefit, upper-bound cost (less redundant)
        AGGRESSIVE      // upper-bound benefit, lower-bound cost (more redundant)
    };

    EvalMode eval_mode;
    static EvalMode DEFAULT_EVAL_MODE;

    unsigned int step;
    class ErrorConfidenceBounds;
    std::vector<ErrorConfidenceBounds *> error_bounds;
    std::map<Estimator *, ErrorConfidenceBounds *> bounds_by_estimator;
    std::map<std::string, Estimator *> estimators_by_name;
    std::map<std::string, ErrorConfidenceBounds *> placeholders;

    BoundType getBoundType(EvalMode eval_mode, Strategy *strategy, 
                           typesafe_eval_fn_t fn);
    double evaluateBounded(BoundType bound_type, typesafe_eval_fn_t fn,
                           void *strategy_arg, void *chooser_arg);

    void *last_chooser_arg;
    std::map<std::pair<Strategy*, typesafe_eval_fn_t>, double> cache;
    void clearCache();

    bool weighted;
};

#endif
