#ifndef _INTNW_JOINT_DISTRIBUTION_H_
#define _INTNW_JOINT_DISTRIBUTION_H_

#include "joint_distribution.h"
#include "small_map.h"
#include "strategy.h"

class Estimator;
class StatsDistribution;

#include <vector>

typedef small_map<Estimator *, StatsDistribution *> EstimatorErrorMap;
typedef small_map<Estimator *, double *> EstimatorErrorValuesMap;
typedef small_map<Estimator *, size_t> EstimatorIndicesMap;

class IntNWJointDistribution : public AbstractJointDistribution {
  public:
    IntNWJointDistribution(EmpiricalErrorEvalMethod eval_method, 
                           const std::vector<Strategy *>& strategies);
    ~IntNWJointDistribution();

    virtual void setEvalArgs(void *strategy_arg_, void *chooser_arg_);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual void observationAdded(Estimator *estimator, double value);
  private:
    void *strategy_arg;
    void *chooser_arg;
    int bytelen;

    EstimatorErrorMap estimatorError;
    
    EstimatorErrorValuesMap estimatorErrorValues;
    EstimatorIndicesMap estimatorIndices;

    std::vector<Strategy *> singular_strategies;
    std::vector<std::vector<Estimator *> > singular_strategy_estimators;
    
    double ***singular_probabilities;
    double ***singular_error_values;
    size_t **singular_error_count;

    double ****singular_strategy_saved_values;

    double singularStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn);
    double redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    double redundantStrategyExpectedValueMin(size_t saved_value_type);
    double redundantStrategyExpectedValueSum(size_t saved_value_type);
    
    void getEstimatorErrorDistributions();
    void clearEstimatorErrorDistributions();

    void ensureValidMemoizedValues(eval_fn_type_t saved_value_type);
};

#endif /* _INTNW_JOINT_DISTRIBUTION_H_ */
