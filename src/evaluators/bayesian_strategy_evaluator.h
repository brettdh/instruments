#ifndef BAYESIAN_STRATEGY_EVALUATOR_H_INCL
#define BAYESIAN_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class Estimator;
class StatsDistributionBinned;

#include <vector>
#include <map>
#include <string>

/* The Bayesian strategy evaluator shares some basic things in common
 * with the brute-force method -- namely, its use of empirical distributions
 * based on the observations and predictions associated with estimators.
 * However, its iteration scope is fundamentally different.
 * Whereas the brute-force method must always iterate over
 * all n samples for each of its m estimator error distributions,
 * which is O(n^m), the Bayesian method need only iterate
 * over the likelihood distribution bins, which is O(mn).
 * Though the joint prior is still O(n^m) in size (conceptual size,
 * not actual memory usage), the likelihood distribution
 * is much more sparse, because a new measurement of an estimated value
 * only adds one new entry, whereas it adds n^(m-1) entries to the
 * joint prior distribution.
 *
 * Therefore, I am implementing this separately from the brute-force method.
 */
class BayesianStrategyEvaluator : public StrategyEvaluator {
  public:
    BayesianStrategyEvaluator(bool weighted_);
    virtual ~BayesianStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);

    virtual void saveToFile(const char *filename);
    virtual void restoreFromFileImpl(const char *filename);
  protected:
    virtual void processObservation(Estimator *estimator, double observation, 
                                    double old_estimate, double new_estimate);
    virtual void setStrategies(const instruments_strategy_t *strategies_,
                               size_t num_strategies_);
    
  private:
    // for answering "which strategy wins?" based only on estimator values
    class SimpleEvaluator;
    SimpleEvaluator *simple_evaluator;

    class Likelihood;
    class DecisionsHistogram;
    Likelihood *likelihood;
    DecisionsHistogram *normalizer;
    
    friend class DistributionKey;

    std::map<Estimator *, StatsDistributionBinned *> estimatorSamples;
    std::map<Estimator *, double> last_estimator_values;

    StatsDistributionBinned *createStatsDistribution(Estimator *estimator);

    Strategy *getBestSingularStrategy(void *chooser_arg);
    bool uninitializedStrategiesExist();

    // A list of all observations, for simplifying save/restore.
    struct stored_observation {
        std::string name;
        double observation;
        double old_estimate;
        double new_estimate;
    };
    std::vector<stored_observation> ordered_observations;
    
    std::map<std::string, Estimator *> estimators_by_name;

    Estimator *getEstimator(const std::string& name);

    void clearDistributions();

    bool weighted;
};

#endif
