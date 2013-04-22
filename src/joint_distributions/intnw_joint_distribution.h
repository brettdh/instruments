#ifndef _INTNW_JOINT_DISTRIBUTION_H_
#define _INTNW_JOINT_DISTRIBUTION_H_

#include "abstract_joint_distribution.h"
#include "small_map.h"
#include "strategy.h"

class Estimator;
class StatsDistribution;

#include <vector>
#include <map>
#include <string>

// TODO: parameterize the second template argument?
// TODO: have a StatsDistribution subclass that 
// TODO: internally calculates the Bayesian probability.
// TODO: actually, I still need a joint distribution
// TODO: (this is the *prior*) in addition to the 
// TODO: likelihood matrix.  So I can reuse this joint distribution
// TODO: for the prior and keep a separate joint distribution
// TODO: for the likelihood values.
// TODO: Now, the likelihood distribution should be 
// TODO: isomorphic to the prior distribution...
// TODO: seems like I should associate those somehow,
// TODO: since they need to be associated when I do the weighted sum.
// TODO: hmmm.....
typedef small_map<Estimator *, StatsDistribution *> EstimatorSamplesMap;
typedef small_map<std::string, StatsDistribution *> EstimatorSamplesPlaceholderMap;
typedef small_map<Estimator *, double *> EstimatorSamplesValuesMap;
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

    virtual void saveToFile(std::ofstream& out);
    virtual void restoreFromFile(std::ifstream& in);
  protected:
    void *strategy_arg;
    void *chooser_arg;

    EstimatorSamplesMap estimatorSamples;
    
    EstimatorSamplesValuesMap estimatorSamplesValues;
    EstimatorIndicesMap estimatorIndices;

    std::vector<Strategy *> singular_strategies;
    std::vector<std::vector<Estimator *> > singular_strategy_estimators;
    
    double ***singular_probabilities;
    double ***singular_samples_values;
    size_t **singular_samples_count;

    double ****singular_strategy_saved_values;

    std::map<std::pair<Strategy *, typesafe_eval_fn_t>, double> cache;

    double singularStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn);
    double redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    virtual double redundantStrategyExpectedValueMin(size_t saved_value_type);
    virtual double redundantStrategyExpectedValueSum(size_t saved_value_type);
    
    void getEstimatorSamplesDistributions();
    void clearEstimatorSamplesDistributions();

    void ensureValidMemoizedValues(eval_fn_type_t saved_value_type);

    EstimatorSamplesPlaceholderMap estimatorSamplesPlaceholders;
    Estimator *getExistingEstimator(const std::string& key);

    void ensureSamplesDistributionExists(Estimator *estimator);

    virtual double getSingularJointProbability(double **strategy_probabilities,
                                               size_t index0, size_t index1);

    virtual void addDefaultValue(Estimator *estimator);
};



#endif /* _INTNW_JOINT_DISTRIBUTION_H_ */
