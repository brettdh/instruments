#ifndef _JOINT_DISTRIBUTION_H_
#define _JOINT_DISTRIBUTION_H_

#include "strategy_evaluation_context.h"
#include "strategy.h"
#include "stats_distribution.h"
#include "small_map.h"
#include <vector>
#include "multi_dimension_array.h"

#include "eval_method.h"

#include "abstract_joint_distribution.h"

class Estimator;


typedef small_map<Estimator *, StatsDistribution *> EstimatorErrorMap;

class GenericJointDistribution : public AbstractJointDistribution {
  public:
    GenericJointDistribution(StatsDistributionType dist_type_,
                      const std::vector<Strategy *>& strategies);
    ~GenericJointDistribution();

    void setEvalArgs(void *strategy_arg_, void *chooser_arg_);
    double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn);

    double getAdjustedEstimatorValue(Estimator *estimator);
    void processObservation(Estimator *estimator, double observation,
                            double old_estimate, double new_estimate);
    void processEstimatorConditionsChange(Estimator *estimator);

    virtual void saveToFile(std::ofstream& out);
    virtual void restoreFromFile(std::ifstream& in);

  private:
    EstimatorErrorMap estimatorError;
    void *strategy_arg;
    void *chooser_arg;

    std::vector<Strategy *> singular_strategies;

    std::vector<MultiDimensionArray<double> *> time_memos;
    std::vector<MultiDimensionArray<double> *> energy_memos;
    std::vector<MultiDimensionArray<double> *> data_memos;

    void setEmptyMemos(Strategy *strategy, const std::vector<size_t>& dimensions);
    void clearMemos();

    double getMemoizedValue(MultiDimensionArray<double> *memo, size_t strategy_index);
    void saveMemoizedValue(MultiDimensionArray<double> *memo, double value);

    int getStrategyIndex(Strategy *strategy);
    std::vector<MultiDimensionArray<double> *>& 
        getMemoList(Strategy *strategy, typesafe_eval_fn_t fn);

    friend double memoized_min_time(StrategyEvaluationContext *ctx, 
                                    void *strategy_arg, void *chooser_arg);
    friend double memoized_total_energy_cost(StrategyEvaluationContext *ctx,
                                             void *strategy_arg, void *chooser_arg);
    friend double memoized_total_data_cost(StrategyEvaluationContext *ctx,
                                           void *strategy_arg, void *chooser_arg);
    friend double memo_saving_fn(StrategyEvaluationContext *ctx,
                                 void *strategy_arg, void *chooser_arg);



    class Iterator {
      public:
        Iterator(GenericJointDistribution *distribution_, Strategy *strategy);
        ~Iterator();
        bool isDone();
        double jointProbability();
        void advance();

        double currentEstimatorError(Estimator *estimator);
        const std::vector<size_t>& strategyPosition(Strategy *strategy);
        const std::vector<size_t>& strategyPosition(size_t strategy_index);
        size_t getStrategyIndex(Strategy *strategy);
        size_t numStrategies();

        double evaluate(typesafe_eval_fn_t fn, void *strategy_arg, void *chooser_arg);

      private:
        void setStrategyPositions(size_t index, size_t value);
        void advancePosition(size_t index);
        void resetPosition(size_t index);

        GenericJointDistribution *distribution;

        std::vector<size_t> position;
        std::vector<size_t> end_position;
        bool done;

        std::vector<Strategy *> strategies;
        std::vector<std::vector<size_t> > strategy_positions;
        std::vector<std::vector<size_t*> > strategy_positions_for_estimator_position;

        size_t num_iterators;
        std::vector<StatsDistribution::Iterator *> iterators;
        small_map<Estimator *, StatsDistribution::Iterator *> errorIterators;
        small_map<Estimator *, size_t> iteratorIndices;

        void setCachedProbabilities(size_t index);
        std::vector<double> cached_probabilities;


        void evaluateRecursive(typesafe_eval_fn_t fn, void *strategy_arg, void *chooser_arg,
                               size_t depth, double& weightedSum, double probability);
        void evaluateLoop(typesafe_eval_fn_t fn, void *strategy_arg, void *chooser_arg,
                          double& weightedSum);


    };

    Iterator *iterator;
};

#endif /* _JOINT_DISTRIBUTION_H_ */
