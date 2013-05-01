#ifndef BAYESIAN_STRATEGY_EVALUATOR_H_INCL
#define BAYESIAN_STRATEGY_EVALUATOR_H_INCL

#include "strategy_evaluator.h"

class Estimator;
class StatsDistributionBinned;

#include <map>

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
    BayesianStrategyEvaluator();
    virtual ~BayesianStrategyEvaluator();

    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg);

    virtual void saveToFile(const char *filename);
    virtual void restoreFromFile(const char *filename);
  protected:
    virtual void observationAdded(Estimator *estimator, double value);
    virtual void setStrategies(const instruments_strategy_t *strategies_,
                               size_t num_strategies_);
    
  private:
    // for answering "which strategy wins?" based only on estimator values
    StrategyEvaluator *simple_evaluator; // will use TRUSTED_ORACLE

    class Likelihood;
    class DecisionsHistogram;
    Likelihood *likelihood;
    DecisionsHistogram *normalizer;

    // last chooser arg passed into expectedValue
    //  (used to make decisions for updating likelihood distribution when
    //   estimator values change).
    void *chooser_arg;
    
    std::map<Estimator *, StatsDistributionBinned *> estimatorSamples;

    StatsDistributionBinned *createStatsDistribution();

    Strategy *getBestSingularStrategy(void *chooser_arg);
};

#endif
