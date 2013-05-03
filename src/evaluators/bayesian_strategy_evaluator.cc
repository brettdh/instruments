#include "bayesian_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution_binned.h"
#include "debug.h"

#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <sstream>
using std::vector; using std::map;
using std::runtime_error; using std::ostringstream;

#include <stdlib.h>
#include <assert.h>
#include <math.h>

typedef double (*combiner_fn_t)(double, double);

class CmpVectors {
    constexpr static double THRESHOLD = 0.0001;
    
  public:
    // return true iff first < second
    bool operator()(const vector<double>& first, const vector<double>& second) const {
        assert(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i) {
            double diff = first[i] - second[i];
            if (fabs(diff) > THRESHOLD) {
                return diff < 0.0;
            }
        }
        return false;
    }
};

class BayesianStrategyEvaluator::Likelihood {
  public:
    Likelihood(BayesianStrategyEvaluator *evaluator_);
    void addDecision(Strategy *winner, Estimator *estimator, double observation);
    double getWeightedSum(Strategy *strategy, typesafe_eval_fn_t fn,
                          void *strategy_arg, void *chooser_arg);
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
    likelihood = new Likelihood(this);
    normalizer = new DecisionsHistogram;
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
        simple_evaluator->setSilent(true);
    }
}

Strategy *
BayesianStrategyEvaluator::getBestSingularStrategy(void *chooser_arg)
{
    auto is_uninitialized = [&](Estimator *estimator) {
        return estimatorSamples.count(estimator) == 0;
    };
    auto has_uninitialized_estimators = [&](Strategy *strategy) {
        if (strategy->isRedundant()) {
            // ignore; captured by child strategies' estimators
            return false;
        };

        auto estimators = strategy->getEstimators();
        return any_of(estimators.begin(), estimators.end(), is_uninitialized);
    };
    if (any_of(strategies.begin(), strategies.end(), has_uninitialized_estimators)) {
        return NULL;
    }
    return (Strategy *) simple_evaluator->chooseStrategy(chooser_arg);
}


double 
BayesianStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // it's bandwidth or latency stored in the distribution, rather
    // than an error value, so just return the value.
    // XXX-BAYESIAN:  yes, this may over-emphasize history.  known (potential) issue.

    return likelihood->getCurrentEstimatorSample(estimator);
}

void 
BayesianStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (estimatorSamples.count(estimator) == 0) {
        estimatorSamples[estimator] = createStatsDistribution(estimator);
    }
    estimatorSamples[estimator]->addValue(value);

    Strategy *winner = getBestSingularStrategy(chooser_arg);
    if (winner) {
        assert(!winner->isRedundant());
        // TrustedOracleStrategyEvaluator will not pick a redundant strategy,
        // because it has the same time as the lowest singular strategy; thus benefit == 0.
    }
    
    likelihood->addDecision(winner, estimator, value);
    normalizer->addDecision(winner);
}

StatsDistributionBinned *
BayesianStrategyEvaluator::createStatsDistribution(Estimator *estimator)
{
    return StatsDistributionBinned::create(estimator);
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
    assert(bestSingular);
    double normalizing_factor = normalizer->getValue(bestSingular);
    dbgprintf("[bayesian] normalizing factor = %f\n", normalizing_factor);
    return normalizing_factor * likelihood->getWeightedSum(strategy, fn, strategy_arg, chooser_arg);
}

BayesianStrategyEvaluator::Likelihood::Likelihood(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_)
{
}

void 
BayesianStrategyEvaluator::Likelihood::addDecision(Strategy *winner, Estimator *estimator, double observation)
{
    last_observation[estimator] = observation;
    
    if (!winner) return;

    // for each strategy:
    //     get the strategy's current key (vector of bins, based on estimator values) 
    //     look up the histogram in that strategy's map
    //     add a count to the decisions histogram
    for (Strategy *strategy : evaluator->strategies) {
        auto& strategy_likelihood = likelihood_per_strategy[strategy];
        vector<double> key = getCurrentEstimatorKey(strategy);
        if (strategy_likelihood.count(key) == 0) {
            strategy_likelihood[key] = new DecisionsHistogram;
        }
        DecisionsHistogram *histogram = strategy_likelihood[key];
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
        assert(last_observation.count(estimator) > 0);
        assert(evaluator->estimatorSamples.count(estimator) > 0);
        
        double obs = last_observation[estimator];
        double bin_mid = evaluator->estimatorSamples[estimator]->getBinnedValue(obs);
        key.push_back(bin_mid);
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
                                                      void *strategy_arg, void *chooser_arg)
{
    Strategy *bestSingular = evaluator->getBestSingularStrategy(chooser_arg);
    assert(bestSingular);
    double weightedSum = 0.0;

    auto& strategy_likelihood = likelihood_per_strategy[strategy];
    for (auto& map_pair : strategy_likelihood) {
        auto key = map_pair.first;
        DecisionsHistogram *histogram = map_pair.second;

        setEstimatorSamples(strategy, key);
        double value = fn(evaluator, strategy_arg, chooser_arg);
        double prior = jointPriorProbability(strategy, key);
        double likelihood_coeff = histogram->getValue(bestSingular);
        
        ostringstream s;
        s << "[bayesian] key: [ ";
        for (double v : key) {
            s << v << " ";
        }
        s << "]  prior: " << prior << "  likelihood_coeff: " << likelihood_coeff;
        dbgprintf("%s\n", s.str().c_str());
        
        weightedSum += (value * prior * likelihood_coeff);
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
    if (!winner) return;
    
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
