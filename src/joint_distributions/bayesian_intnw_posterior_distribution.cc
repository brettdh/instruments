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

double 
BayesianIntNWPosteriorDistribution::redundantStrategyExpectedValueMin(size_t saved_value_type)
{
    return 0.0; // TODO
}

double 
BayesianIntNWPosteriorDistribution::redundantStrategyExpectedValueSum(size_t saved_value_type)
{
    return 0.0; // TODO
}
