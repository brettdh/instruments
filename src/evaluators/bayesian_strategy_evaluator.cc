#include "bayesian_strategy_evaluator.h"
#include "bayesian_intnw_posterior_distribution.h"
#include "estimator.h"

#include <stdlib.h>

typedef double (*combiner_fn_t)(double, double);

template <double threshold>
static int cmp_vectors(const vector<double>& first, const vector<double>& second)
{
    assert(first.size() == second.size());
    for (size_t i = 0; i < first.size(); ++i) {
        double diff = first[i] - second[i];
        if (fabs(first[i] - second[i]) > threshold) {
            return (diff < 0.0 ? -1 : 1);
        }
    }
    return 0;
}

class BayesianStrategyEvaluator::Likelihood {
  public:
    BayesianLikelihood(BayesianStrategyEvaluator *evaluator_);
    void addDecision(Strategy *winner, Estimator *estimator, double value);
    double getWeightedSum(Strategy *strategy, Strategy *best_singular, combiner_fn_t combiner=NULL);
  private:
    BayesianStrategyEvaluator *evaluator;
    map<vector<double>, DecisionsHistogram *, cmp_vectors<0.0001> > distribution;
    map<Estimator *, double> lastObservation;
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
        simple_evaluator = new TrustedOracleStrategyEvaluator;
    }
    simple_evaluator->setStrategies(new_strategies, num_strategies);
}

Strategy *
BayesianStrategyEvaluator::getBestSingularStrategy(void *chooser_arg)
{
    return simple_evaluator->chooseStrategy(chooser_arg);
}


double 
BayesianStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // TODO-BAYESIAN: specifically, it's bandwidth or latency stored in the
    // TODO-BAYESIAN: distribution, rather than an error value, so
    // TODO-BAYESIAN: just return the value.
    // XXX-BAYESIAN:  yes, this may over-emphasize history.  known (potential) issue.

    double *estimator_sample_values = estimatorSamplesValues[estimator];
    size_t index = estimatorIndices[estimator];
    double sample = estimator_sample_values[index];
    return sample;
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


double
BayesianStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                         void *strategy_arg, void *chooser_arg)
{
    // TODO: here is the crux of the loop.
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

double 
BayesianIntNWPosteriorDistribution::redundantStrategyExpectedValueMin(size_t saved_value_type)
{
    return likelihood->getWeightedSum(bestSingularStrategy, min) * normalizer->getValue(bestSingularStrategy);
}

double 
BayesianIntNWPosteriorDistribution::redundantStrategyExpectedValueSum(size_t saved_value_type)
{
    return likelihood->getWeightedSum(bestSingularStrategy, sum) * normalizer->getValue(bestSingularStrategy);
}

BayesianStrategyEvaluator::BayesianLikelihood::BayesianLikelihood(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_)
{
    
}


void 
BayesianStrategyEvaluator::Likelihood::addDecision(Strategy *winner, Estimator *estimator, double observation)
{
    // TODO: update correct count in correct bin of the likelihood distribution.
    lastObservation[estimator] = observation;

    // for each strategy:
    //     get the strategy's current key (vector of bins, based on estimator values) 
    //     look up the histogram in that strategy's map
    for (Strategy *strategy : strategies) {
        vector<double> key = getCurrentEstimatorKey(strategy);
        if (likelihood.count(key) == 0) {
            likelihood[key] = new DecisionsHistogram;
        }
        DecisionsHistogram *histogram = likelihood[key];
        assert(histogram);
        histogram->addDecision(winner);
    }
}

double 
BayesianStrategyEvaluator::Likelihood::getWeightedSum(Strategy *strategy, Strategy *bestSingular, combiner_fn_t combiner)
{
    if (strategy->isRedundant()) {
        // TODO: iterate over child strategies, combine sums
    } else {
        // TODO: iterate over likelihood pairs
    }
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
    return decision[winner] / ((double) total);
}
