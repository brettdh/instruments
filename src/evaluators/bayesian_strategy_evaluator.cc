#include "bayesian_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution_binned.h"

#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
using std::vector; using std::map;
using std::runtime_error;

#include <stdlib.h>
#include <assert.h>
#include <math.h>

typedef double (*combiner_fn_t)(double, double);

class CmpVectors {
    constexpr static double THRESHOLD = 0.0001;
    
  public:
    int operator()(const vector<double>& first, const vector<double>& second) const {
        assert(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i) {
            double diff = first[i] - second[i];
            if (fabs(first[i] - second[i]) > THRESHOLD) {
                return (diff < 0.0 ? -1 : 1);
            }
        }
        return 0;
    }
};

class BayesianStrategyEvaluator::Likelihood {
  public:
    Likelihood(BayesianStrategyEvaluator *evaluator_);
    void addDecision(Strategy *winner, Estimator *estimator, double value);
    double getWeightedSum(Strategy *strategy, typesafe_eval_fn_t fn,
                          void *strategy_arg, void *chooser_arg, combiner_fn_t combiner);
    double getCurrentEstimatorSample(Estimator *estimator);
  private:
    BayesianStrategyEvaluator *evaluator;
    map<Estimator *, double> last_observation;

    // for use during iterated weighted sum.
    map<Estimator *, double> currentEstimatorSamples;
    
    // Each value_type is a map of likelihood-value-tuples to 
    //   their likeihood decision histograms.
    // I keep these separately for each strategy for easy lookup
    //   and iteration over only those estimators that matter
    //   for the current strategy.
    map<Strategy *, map<vector<double>, DecisionsHistogram *, CmpVectors> > likelihood_per_strategy;

    vector<double> getCurrentEstimatorKey(Strategy *strategy);
    void forEachEstimator(Strategy *strategy, const vector<double>& key,
                          std::function<void(Estimator *, double)> fn);
    void setEstimatorSamples(Strategy *strategy, const vector<double>& key);
    double jointPriorProbability(Strategy *strategy, const vector<double>& key);
};

class BayesianStrategyEvaluator::DecisionsHistogram {
  public:
    DecisionsHistogram();
    void addDecision(Strategy *winner);
    double getValue(Strategy *winner);
  private:
    size_t total;
    map<Strategy *, size_t> decisions;
};

BayesianStrategyEvaluator::BayesianStrategyEvaluator()
    : simple_evaluator(NULL)
{
}

BayesianStrategyEvaluator::~BayesianStrategyEvaluator()
{
}

void
BayesianStrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                         size_t num_strategies)
{
    StrategyEvaluator::setStrategies(new_strategies, num_strategies);
    if (!simple_evaluator) {
        simple_evaluator = StrategyEvaluator::create(new_strategies, num_strategies, 
                                                     TRUSTED_ORACLE);
    }
}

Strategy *
BayesianStrategyEvaluator::getBestSingularStrategy(void *chooser_arg)
{
    return (Strategy *) simple_evaluator->chooseStrategy(chooser_arg);
}


double 
BayesianStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // TODO-BAYESIAN: specifically, it's bandwidth or latency stored in the
    // TODO-BAYESIAN: distribution, rather than an error value, so
    // TODO-BAYESIAN: just return the value.
    // XXX-BAYESIAN:  yes, this may over-emphasize history.  known (potential) issue.

    return likelihood->getCurrentEstimatorSample(estimator);
}

void 
BayesianStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (estimatorSamples.count(estimator) == 0) {
        estimatorSamples[estimator] = createStatsDistribution();
    }
    estimatorSamples[estimator]->addValue(value);

    Strategy *winner = getBestSingularStrategy(chooser_arg);
    assert(!winner->isRedundant());
    // TrustedOracleStrategyEvaluator will not pick a redundant strategy,
    // because it has the same time as the lowest singular strategy; thus benefit == 0.

    likelihood->addDecision(winner, estimator, value);
    normalizer->addDecision(winner);
}

StatsDistributionBinned *
BayesianStrategyEvaluator::createStatsDistribution()
{
    // TODO: choose bin size.
    return new StatsDistributionBinned;
}


#include <functional>

static inline double min(double a, double b)
{
    return (a < b) ? a : b;
}

static inline double sum(double a, double b)
{
    return a + b;
}

combiner_fn_t
get_combiner_fn(typesafe_eval_fn_t fn)
{
    if (fn == redundant_strategy_minimum_time) {
        return min;
    } else if (fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost) {
        return sum;
    } else {
        return NULL;
    }
}

double
BayesianStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                         void *strategy_arg, void *chooser_arg_)
{
    chooser_arg = chooser_arg_;
    Strategy *bestSingular = getBestSingularStrategy(chooser_arg);
    double normalizing_factor = normalizer->getValue(bestSingular);
    return likelihood->getWeightedSum(strategy, fn, strategy_arg, chooser_arg,
                                      get_combiner_fn(fn)) * normalizing_factor;
}

BayesianStrategyEvaluator::Likelihood::Likelihood(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_)
{
}

void 
BayesianStrategyEvaluator::Likelihood::addDecision(Strategy *winner, Estimator *estimator, double observation)
{
    last_observation[estimator] = observation;

    // for each strategy:
    //     get the strategy's current key (vector of bins, based on estimator values) 
    //     look up the histogram in that strategy's map
    //     add a count to the decisions histogram
    for (Strategy *strategy : evaluator->strategies) {
        auto& likelihood = likelihood_per_strategy[strategy];
        vector<double> key = getCurrentEstimatorKey(strategy);
        if (likelihood.count(key) == 0) {
            likelihood[key] = new DecisionsHistogram;
        }
        DecisionsHistogram *histogram = likelihood[key];
        assert(histogram);
        histogram->addDecision(winner);
    }
}

void
BayesianStrategyEvaluator::Likelihood::forEachEstimator(Strategy *strategy, const vector<double>& key,
                                                        std::function<void(Estimator *, double)> fn)
{
    vector<Estimator *> estimators = strategy->getEstimators();
    assert(estimators.size() == key.size());
    int i = 0;
    for (double sample : key) {
        Estimator *estimator = estimators[i++];
        fn(estimator, sample);
    }
}

void
BayesianStrategyEvaluator::Likelihood::setEstimatorSamples(Strategy *strategy, const vector<double>& key)
{
    forEachEstimator(strategy, key, [&](Estimator *estimator, double sample) {
            currentEstimatorSamples[estimator] = sample;
        });
}

vector<double>
BayesianStrategyEvaluator::Likelihood::getCurrentEstimatorKey(Strategy *strategy)
{
    vector<Estimator *> estimators = strategy->getEstimators();
    vector<double> key;
    for (Estimator *estimator : estimators) {
        key.push_back(estimator->getEstimate());
    }
    return key;
}

double 
BayesianStrategyEvaluator::Likelihood::getCurrentEstimatorSample(Estimator *estimator)
{
    assert(currentEstimatorSamples.count(estimator) > 0);
    return currentEstimatorSamples[estimator];
}


double
BayesianStrategyEvaluator::Likelihood::jointPriorProbability(Strategy *strategy, const vector<double>& key)
{
    double probability = 1.0;
    forEachEstimator(strategy, key, [&](Estimator *estimator, double sample) {
            assert(evaluator->estimatorSamples.count(estimator) > 0);
            probability *= evaluator->estimatorSamples[estimator]->getProbability(sample);
        });
    return probability;
}

double 
BayesianStrategyEvaluator::Likelihood::getWeightedSum(Strategy *strategy, typesafe_eval_fn_t fn,
                                                      void *strategy_arg, void *chooser_arg, combiner_fn_t combiner)
{
    Strategy *bestSingular = evaluator->getBestSingularStrategy(chooser_arg);
    double weightedSum = 0.0;

    auto& likelihood = likelihood_per_strategy[strategy];
    for (auto& map_pair : likelihood) {
        auto key = map_pair.first;
        DecisionsHistogram *histogram = map_pair.second;

        double combined = 0.0;
        bool first = true;
        for (Strategy *cur_strategy : evaluator->strategies) {
            setEstimatorSamples(cur_strategy, key);
            double value = fn(evaluator, strategy_arg, chooser_arg);
            if (first) {
                first = false;
                combined = value;
            } else {
                combined = combiner(combined, value);
            }
        }
        weightedSum += (combined
                        * jointPriorProbability(strategy, key)
                        * histogram->getValue(bestSingular));
    }
    return weightedSum;
}


BayesianStrategyEvaluator::DecisionsHistogram::DecisionsHistogram()
    : total(0)
{
}

void
BayesianStrategyEvaluator::DecisionsHistogram::addDecision(Strategy *winner)
{
    if (decisions.count(winner) == 0) {
        decisions[winner] = 0;
    }
    decisions[winner]++;
    total++;
}

double
BayesianStrategyEvaluator::DecisionsHistogram::getValue(Strategy *winner)
{
    assert(total > 0);
    return decisions[winner] / ((double) total);
}


void
BayesianStrategyEvaluator::saveToFile(const char *filename)
{
    // TODO
    throw runtime_error("NOT IMPLEMENTED");
}

void
BayesianStrategyEvaluator::restoreFromFile(const char *filename)
{
    // TODO
    throw runtime_error("NOT IMPLEMENTED");
}
