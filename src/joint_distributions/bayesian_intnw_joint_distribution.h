#ifndef _BAYESIAN_INTNW_JOINT_DISTRIBUTION_H_
#define _BAYESIAN_INTNW_JOINT_DISTRIBUTION_H_

#include "abstract_joint_distribution.h"
#include "small_map.h"
#include "strategy.h"

class Estimator;
class StatsDistribution;

#include <vector>
#include <map>
#include <string>

class BayesianIntNWJointDistribution : public AbstractJointDistribution {
  public:
    BayesianIntNWJointDistribution(EmpiricalErrorEvalMethod eval_method, 
                                   const std::vector<Strategy *>& strategies);
    ~BayesianIntNWJointDistribution();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual void observationAdded(Estimator *estimator, double value);

    virtual void saveToFile(std::ofstream& out);
    virtual void restoreFromFile(std::ifstream& in);
  protected:
    virtual double redundantStrategyExpectedValueMin(size_t saved_value_type);
    virtual double redundantStrategyExpectedValueSum(size_t saved_value_type);

    virtual double getSingularJointProbability(double **strategy_probabilities,
                                               size_t index0, size_t index1);

    virtual void addDefaultValue(Estimator *estimator);
};

#endif /* _BAYESIAN_JOINT_DISTRIBUTION_H_ */
