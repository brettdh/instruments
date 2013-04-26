#include "bayesian_intnw_posterior_distribution.h"

#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "estimator.h"
#include "error_calculation.h"
#include "debug.h"

#include <stdlib.h>
#include <stdio.h>
#include <float.h>
#include <assert.h>

#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <string>
#include <stdexcept>
using std::map; using std::pair; using std::make_pair;
using std::vector; using std::ifstream; using std::ofstream; using std::find_if;
using std::ostringstream; using std::endl;
using std::runtime_error; using std::string;

typedef double (*combiner_fn_t)(double, double);

class BayesianLikelihood {
  public:
    BayesianLikelihood(BayesianIntNWPosteriorDistribution *parent);
    void addDecision(Strategy *winner);
    double getWeightedSumSingular(Strategy *strategy, combiner_fn_t combiner);
    double getWeightedSumRedundant(Strategy *best_singular, combiner_fn_t combiner);
};

class BayesianNormalizer {
  public:
    void addDecision(Strategy *winner);
    double getValue(Strategy *winner);
};

BayesianIntNWPosteriorDistribution::
BayesianIntNWPosteriorDistribution(const std::vector<Strategy *>& strategies)
    : IntNWJointDistribution(BINNED, strategies)
{
}

BayesianIntNWPosteriorDistribution::~BayesianIntNWPosteriorDistribution()
{
}

void BayesianIntNWPosteriorDistribution::addDefaultValue(Estimator *estimator)
{
    // samples are measurement values, so there's no default value;
    // only get one when we get a measurement (unlike error, where we need
    // to initially have a no-error sample in the distribution
}


double
BayesianIntNWPosteriorDistribution::getSingularJointProbability(double **strategy_probabilities,
                                                                size_t index0, size_t index1)
{
    return 0.0; // TODO
}


double
BayesianIntNWPosteriorDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    // TODO-BAYESIAN: override this (or part of it) in a subclass to implement the 
    // TODO-BAYESIAN: relevant part of the posterior distribution calculation.
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
BayesianIntNWPosteriorDistribution::observationAdded(Estimator *estimator, double value)
{
    // TODO-BAYESIAN: override this (or part of it) in a subclass to implement the 
    // TODO-BAYESIAN: relevant part of the posterior distribution calculation.
    // TODO-BAYESIAN: specifically, update the empirically tracked parts of
    // TODO-BAYESIAN: the posterior distribution.

    if (estimatorSamples.count(estimator) == 0) {
        ensureSamplesDistributionExists(estimator);
    }
    estimatorSamples[estimator]->addValue(value);
    clearEstimatorSamplesDistributions();

    Strategy *winner = evaluator->getBestSingularStrategy(chooser_arg);
    likelihood->addDecision(winner);
    normalizer->addDecision(winner);
}

void
BayesianIntNWPosteriorDistribution::saveToFile(ofstream& out)
{
    // TODO-BAYESIAN: override this (or part of it) in a subclass to implement the 
    // TODO-BAYESIAN: relevant part of the posterior distribution calculation.

    try {
        out << estimatorSamples.size() << " estimators" << endl;
        for (EstimatorSamplesMap::iterator it = estimatorSamples.begin();
             it != estimatorSamples.end(); ++it) {
            Estimator *estimator = it->first;
            StatsDistribution *dist = it->second;
            
            dist->appendToFile(estimator->getName(), out);
        }
    } catch (runtime_error& e) {
        dbgprintf("WARNING: failed to save joint distribution: %s\n", e.what());
    }
}

void 
BayesianIntNWPosteriorDistribution::restoreFromFile(ifstream& in)
{
    // TODO-BAYESIAN: override this (or part of it) in a subclass to implement the 
    // TODO-BAYESIAN: relevant part of the posterior distribution calculation.

    try {
        size_t num_estimators = 0;
        string dummy;
        check(in >> num_estimators >> dummy, "Parse failure");
        
        for (size_t i = 0; i < num_estimators; ++i) {
            string key;
            StatsDistribution *dist = createSamplesDistribution();
            key = dist->restoreFromFile(in);
            
            Estimator *estimator = getExistingEstimator(key);
            if (estimator) {
                assert(estimatorSamples.count(estimator) > 0);
                delete estimatorSamples[estimator];
                estimatorSamples[estimator] = dist;
            } else {
                estimatorSamplesPlaceholders[key] = dist;
            }
        }

        clearEstimatorSamplesDistributions();
    } catch (runtime_error& e) {
        dbgprintf("WARNING: failed to restore joint distribution: %s\n", e.what());
    }
}

static inline double
get_one_redundant_probability(double ***singular_probabilities,
                              size_t strategy_index,
                              size_t estimator_index,
                              size_t distribution_index)
{
    return singular_probabilities[strategy_index][estimator_index][distribution_index];
}

inline double
BayesianIntNWPosteriorDistribution::
bayesianJointProbability(size_t i, size_t j, size_t k, size_t m)
{
    // indices point into the prior and the likelihood distributions.

    // Bayesian posterior probability calculation:
    // posterior = prior * likelihood / normalizer
    // prior is calculated partially at each loop level, by get_one_redundant_probability
    // here we return likelihood, which gets multiplied into the prior
    // inside the loop.
    // the normalizing factor doesn't depend on the estimator distribution,
    // so it can be pulled out of the loop and applied later.
    return likelihood->getValue(chosenSingularStrategy, i, j, k, m);
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

BayesianLikelihood::BayesianLikelihood(BayesianIntNWPosteriorDistribution *parent_)
    : parent(parent_)
{
    
}


void 
BayesianLikelihood::addDecision(Strategy *winner)
{

}

double 
BayesianLikelihood::getWeightedSumSingular(Strategy *winner, combiner_fn_t combiner)
{

}

double 
BayesianLikelihood::getWeightedSumRedundant(Strategy *winner, combiner_fn_t combiner)
{

}

void
BayesianNormalizer::addDecision(Strategy *winner)
{
}

double
BayesianNormalizer::getValue(Strategy *winner)
{
}
