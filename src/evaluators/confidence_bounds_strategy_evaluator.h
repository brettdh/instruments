#ifndef CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL
#define CONFIDENCE_BOUNDS_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

#include <string>
#include <vector>
#include <map>
#include <tuple>

class ConfidenceBoundsStrategyEvaluator : public StrategyEvaluator {
  public:
    ConfidenceBoundsStrategyEvaluator(bool weighted_);
    virtual ~ConfidenceBoundsStrategyEvaluator();
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg, 
                                 ComparisonType comparison_type);
    virtual bool singularComparisonIsDifferent();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);

    virtual void saveToFile(const char *filename);
    virtual void restoreFromFileImpl(const char *filename);
  protected:
    virtual void processObservation(Estimator *estimator, double observation,
                                    double old_estimate, double new_estimate);
    virtual void processEstimatorConditionsChange(Estimator *estimator);
    virtual void processEstimatorReset(Estimator *estimator, const char *filename);
  private:
    enum BoundType {
        LOWER=0, UPPER, CENTER
    };

    enum EvalMode {
        CONSERVATIVE=0, // lower-bound benefit, upper-bound cost (less redundant)
        AGGRESSIVE      // upper-bound benefit, lower-bound cost (more redundant)
    };

    EvalMode eval_mode;
    static EvalMode DEFAULT_EVAL_MODE;

    int step;
    class ErrorConfidenceBounds;
    std::vector<ErrorConfidenceBounds *> error_bounds;
    std::map<Estimator *, ErrorConfidenceBounds *> bounds_by_estimator;
    std::map<std::string, Estimator *> estimators_by_name;
    std::map<std::string, ErrorConfidenceBounds *> placeholders;

    BoundType getBoundType(EvalMode eval_mode, Strategy *strategy, 
                           typesafe_eval_fn_t fn, ComparisonType comparison_type);
    double evaluateBounded(BoundType bound_type, typesafe_eval_fn_t fn,
                           void *strategy_arg, void *chooser_arg);

    ErrorConfidenceBounds *getBounds(Estimator *estimator);

    void setConditionalBounds();
    void clearConditionalBounds();

    void *last_chooser_arg;
    std::map<std::tuple<Strategy*, typesafe_eval_fn_t, ComparisonType>, double> cache;
    void clearCache();

    virtual void restoreFromFileImpl(const char *filename, const std::string& estimator_name);

    bool weighted;
};

#endif
