#ifndef _OPTIMIZED_GENERIC_JOINT_DISTRIBUTION_H_
#define _OPTIMIZED_GENERIC_JOINT_DISTRIBUTION_H_

#include "abstract_joint_distribution.h"
#include "small_map.h"
#include "strategy.h"

#include <nested_loop.h>


class Estimator;
class StatsDistribution;

#include <vector>
#include <map>
#include <string>
#include <memory>

class OptimizedGenericJointDistribution : public AbstractJointDistribution {
  public:
    typedef small_map<Estimator *, StatsDistribution *> EstimatorSamplesMap;
    typedef small_map<std::string, StatsDistribution *> EstimatorSamplesPlaceholderMap;
    typedef small_map<Estimator *, std::vector<double> *> EstimatorSamplesValuesMap;
    typedef small_map<Estimator *, std::vector<double> > EstimatorErrorAdjustedEstimatesMap;
    typedef small_map<Estimator *, size_t> EstimatorIndicesMap;

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

    void processEstimatorReset(Estimator *estimator, const char *filename);
  protected:
    void *strategy_arg;
    void *chooser_arg;

    EstimatorSamplesMap estimatorSamples;
    
    EstimatorSamplesValuesMap estimatorSamplesValues;
    EstimatorIndicesMap estimatorIndices;
    EstimatorErrorAdjustedEstimatesMap estimatorErrorAdjustedValues;

    std::vector<Strategy *> strategies;
    std::vector<std::vector<Estimator *> > strategy_estimators;
    
    // for the brute-force nested loop
    std::vector<NestedLoop> loops;
    
    typedef std::vector<std::vector<std::vector< double> > > StrategyEstimatorSamples;
    StrategyEstimatorSamples probabilities;
    StrategyEstimatorSamples samples_values;

    void getEstimatorSamplesDistributions();
    void clearEstimatorSamplesDistributions();

    EstimatorSamplesPlaceholderMap estimatorSamplesPlaceholders;
    Estimator *getExistingEstimator(const std::string& key);

    void ensureSamplesDistributionExists(Estimator *estimator);

    virtual void addDefaultValue(Estimator *estimator);

 private:
    friend class ExpectedValueLoop;
    void restoreFromFile(std::ifstream& in, const std::string& estimator_name);
};



#endif /* _INTNW_JOINT_DISTRIBUTION_H_ */
