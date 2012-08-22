#ifndef _JOINT_DISTRIBUTION_H_
#define _JOINT_DISTRIBUTION_H_

#include "strategy.h"
#include "small_set.h"

class Estimator;
class StatsDistribution;
class MultiDimensionArray;

typedef small_map<Estimator *, StatsDistribution *> EstimatorErrorMap;

class JointDistribution : public StrategyEvaluationContext {
  public:
    JointDistribution(const small_set<Strategy *>& strategies);

    double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                         void *strategy_arg, void *chooser_arg);

    double getAdjustedEstimatorValue(Estimator *estimator);
    void observationAdded(Estimator *estimator, double value);
  private:
    EstimatorErrorMap estimatorError;

    std::vector<Strategy *> singular_strategies;

    std::vector<MultiDimensionArray *> time_memos;
    std::vector<MultiDimensionArray *> energy_memos;
    std::vector<MultiDimensionArray *> data_memos;

    class Iterator {
      public:
        Iterator(JointDistribution *distribution, Strategy *strategy);
        ~Iterator();
        bool isDone();
        double probability();
        void advance();

        double currentEstimatorError(Estimator *estimator);
        const vector<size_t>& strategyPosition(size_t strategy_index);
        
      private:
        
    };
};

#endif /* _JOINT_DISTRIBUTION_H_ */
