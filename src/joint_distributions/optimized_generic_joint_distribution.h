#ifndef _OPTIMIZED_GENERIC_JOINT_DISTRIBUTION_H_
#define _OPTIMIZED_GENERIC_JOINT_DISTRIBUTION_H_

#include "abstract_joint_distribution.h"
#include "small_map.h"
#include "strategy.h"

class Estimator;
class StatsDistribution;

#include <vector>
#include <map>
#include <string>

typedef small_map<Estimator *, StatsDistribution *> EstimatorSamplesMap;
typedef small_map<std::string, StatsDistribution *> EstimatorSamplesPlaceholderMap;
typedef small_map<Estimator *, double *> EstimatorSamplesValuesMap;
typedef small_map<Estimator *, size_t> EstimatorIndicesMap;

class OptimizedGenericJointDistribution : public AbstractJointDistribution {
  public:
    OptimizedGenericJointDistribution(StatsDistributionType dist_type, 
                                      const std::vector<Strategy *>& strategies);
    ~OptimizedGenericJointDistribution();

    virtual void setEvalArgs(void *strategy_arg_, void *chooser_arg_);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual void processObservation(Estimator *estimator, double observation,
                                    double old_estimate, double new_estimate);
    virtual void processEstimatorConditionsChange(Estimator *estimator);

    virtual void saveToFile(std::ofstream& out);
    virtual void restoreFromFile(std::ifstream& in);
  protected:
    void *strategy_arg;
    void *chooser_arg;

    EstimatorSamplesMap estimatorSamples;
    
    EstimatorSamplesValuesMap estimatorSamplesValues;
    EstimatorIndicesMap estimatorIndices;

    std::vector<Strategy *> strategies;
    std::vector<std::vector<Estimator *> > strategy_estimators;

    void getEstimatorSamplesDistributions();
    void clearEstimatorSamplesDistributions();

    void ensureValidMemoizedValues(eval_fn_type_t saved_value_type);

    EstimatorSamplesPlaceholderMap estimatorSamplesPlaceholders;
    Estimator *getExistingEstimator(const std::string& key);

    void ensureSamplesDistributionExists(Estimator *estimator);

    virtual void addDefaultValue(Estimator *estimator);

    //void FN_BODY_WITH_COMBINER(double& weightedSum, double (*COMBINER)(double, double), size_t saved_value_type);

};



#endif /* _INTNW_JOINT_DISTRIBUTION_H_ */
