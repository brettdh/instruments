#include "intnw_joint_distribution.h"

#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "estimator.h"
#include "error_calculation.h"
#include "debug.h"
namespace inst = instruments;
using inst::ERROR; using inst::INFO; using inst::DEBUG;

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

static const size_t WIFI_STRATEGY_INDEX = 0;
static const size_t CELLULAR_STRATEGY_INDEX = 1;

static const size_t WIFI_ESTIMATORS_WITH_SESSIONS = 3;
static size_t NUM_ESTIMATORS_SINGULAR[] = {2, 2};

static const size_t REDUNDANT_STRATEGY_CHILDREN = 2;

static const size_t NUM_STRATEGIES = 3;


static void adjust_probs_for_estimator_conditions(Estimator *estimator, double *& values, double *& probs, size_t& count)
{
    size_t index = count;
    size_t pruned_samples = 0;
    
    for (size_t i = 0; i < count; ++i) {
        double error_value = values[i];
        double error_adjusted_estimate = adjusted_estimate(estimator->getEstimate(), error_value);
        
        if (!estimator->valueMeetsConditions(error_adjusted_estimate)) {
            probs[i] = 0.0;
            index = i;
            ++pruned_samples;
        }
    }
    if (pruned_samples == count) {
        // none of my samples met the conditions,
        // so assume that I'm observing a new extreme.
        // Don't store this as an error measurement, because
        // the app hasn't sent me a new measurement yet.
        // Just use it in the current conditional calculation, 
        //  so that the distribution isn't empty.
        // This is probably correct anyway, since the condition being set
        //  means that the app is experiencing an extreme, which often
        //  should cause redundancy (e.g. IntNW, higher-than-usual RTT).
        if (!estimator->hasConditions()) {
            // we must not have any error samples yet.
            return;
        }
        double value = estimator->getConditionalBound();
        double error_value = calculate_error(estimator->getEstimate(), value);
        values[index] = error_value;
        probs[index] = 1.0;
    }
    
    // normalize the array, since estimator error values might have been filtered
    double prob_sum = 0.0;
    for (size_t i = 0; i < count; ++i) {
        prob_sum += probs[i];
    }
    for (size_t i = 0; i < count; ++i) {
        probs[i] /= prob_sum;
    }
}

IntNWJointDistribution::IntNWJointDistribution(StatsDistributionType dist_type,
                                               const std::vector<Strategy *>& strategies)
    : AbstractJointDistribution(dist_type)
{
    strategy_arg = NULL;
    chooser_arg = NULL;
    
}

IntNWJointDistribution::~IntNWJointDistribution()
{
}

void
IntNWJointDistribution::ensureSamplesDistributionExists(Estimator *estimator)
{
    string key = estimator->getName();
    if (estimatorSamplesPlaceholders.count(key) > 0) {
        estimatorSamples[estimator] = estimatorSamplesPlaceholders[key];
        estimatorSamplesPlaceholders.erase(key);
    } else if (estimatorSamples.count(estimator) == 0) {
        estimatorSamples[estimator] = createSamplesDistribution();
        addDefaultValue(estimator);
   }
    
    assert(estimatorSamples.count(estimator) > 0);
}

void
IntNWJointDistribution::addDefaultValue(Estimator *estimator)
{
    // don't add a real error value to the distribution.
    // there's no error until we have at least two observations.
    estimatorSamples[estimator]->addValue(no_error_value());
}

void
IntNWJointDistribution::getEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        if (probabilities[i][0] != NULL) {
            return;
        }

        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            Estimator *estimator = strategy_estimators[i][j];
            ensureSamplesDistributionExists(estimator);
            StatsDistribution *distribution = estimatorSamples[estimator];
            ASSERT(distribution != NULL);
            size_t count = 0, count_probs = 42;
            probabilities[i][j] = get_estimator_samples_probs(distribution, count_probs);
            samples_values[i][j] = get_estimator_samples_values(distribution, count);
            assert(count == count_probs);

            adjust_probs_for_estimator_conditions(estimator, 
                                                  samples_values[i][j],
                                                  probabilities[i][j], count);

            samples_count[i][j] = count;
        }
    }

    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        for (size_t j = 0; j < num_estimators[i]; ++j) {
            Estimator *estimator = strategy_estimators[i][j];
            estimatorSamplesValues[estimator] = samples_values[i][j];
            estimatorIndices[estimator] = 0;
        }
    }
}

void 
IntNWJointDistribution::clearEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        if (probabilities[i][0] == NULL) {
            return;
        }
        
        
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            delete [] probabilities[i][j];
            delete [] samples_values[i][j];
            probabilities[i][j] = NULL;
            samples_values[i][j] = NULL;
            
            samples_count[i][j] = 0;
        }
    }
    estimatorSamplesValues.clear();
    estimatorIndices.clear();
    cache.clear();
}

void 
IntNWJointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearEstimatorSamplesDistributions();
    }

    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
IntNWJointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    pair<Strategy *, typesafe_eval_fn_t> key = make_pair(strategy, fn);
    if (cache.count(key) == 0) {
        if (strategy->isRedundant()) {
            assert(strategy->childrenAreDisjoint(fn));
            cache[key] = redundantStrategyExpectedValue(strategy, fn);
        } else {
            cache[key] = singularStrategyExpectedValue(strategy, fn);
        }
    }
    return cache[key];
}



double 
IntNWJointDistribution::singularStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    getEstimatorSamplesDistributions();

    bool wifi = false;
    double **strategy_probabilities = NULL;
    size_t *samples_count = NULL;
    vector<Estimator *> current_strategy_estimators;
    size_t strategy_index;
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (strategy == strategies[i]) {
            strategy_index = i;
            current_strategy_estimators = strategy_estimators[i];
            samples_count = samples_count[i];
            strategy_probabilities = probabilities[i];
            
            break;
        }
    }
    assert(samples_count);
    
    double weightedSum = 0.0;
    
    size_t max_i, max_j, max_k=0;
    max_i = samples_count[0];
    max_j = samples_count[1];
    
    if (wifi && wifi_uses_sessions) {
        max_k = samples_count[2];
    }

    if (inst::is_debugging_on(DEBUG)) {
        inst::dbgprintf(DEBUG, "strategy \"%s\" (fn %s) uses %zu estimators\n", 
                        strategy->getName(), get_value_name(strategy, fn).c_str(), 
                        current_strategy_estimators.size());
        const size_t sizes[] = { max_i, max_j, max_k };
        for (size_t m = 0; m < ((wifi && wifi_uses_sessions) ? 3 : 2); ++m) {
            Estimator *estimator = current_strategy_estimators[m];
            ostringstream s;
            s << "  " << estimator->getName() << ": ";
            for (size_t n = 0; n < sizes[m]; ++n) {
                estimatorIndices[estimator] = n;
                s << getAdjustedEstimatorValue(estimator) << " ";
            }
            inst::dbgprintf(DEBUG, "%s\n", s.str().c_str());
        }
    }

    AbstractNestedLoop::Looper loop_body = [&](vector<size_t>& indices) {
        double probability = 1.0;
        for (size_t i = 0; i < indices.size(); ++i) {
            estimatorIndices[current_strategy_estimators[i]] = indices[i];
            probability *= strategy_probabilities[i][indices[i]];
        }
        double value = fn(this, strategy_arg, chooser_arg);
        weightedSum += value * probability;
    }

    size_t strategy_index = getStrategyIndex(strategy);
    loops[srategy_index]->run_loop(loop_body);
    return weightedSum;
}

void
IntNWJointDistribution::ensureValidMemoizedValues()
{
    size_t max_i, max_j, max_k=0, max_m, max_n;  /* counts for: */
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */
    max_j = singular_samples_count[0][1]; /* (strategy 0, estimator 1) */
    if (wifi_uses_sessions) {
        max_k = singular_samples_count[0][2]; /* (strategy 0, estimator 2) */
    }
    max_m = singular_samples_count[1][0]; /* (strategy 1, estimator 3) */
    max_n = singular_samples_count[1][1]; /* (strategy 1, estimator 4) */

}

double
IntNWJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    double estimate = estimator->getEstimate();
    if (estimatorSamplesValues.count(estimator) == 0) {
        return estimate;
    }
    double *estimator_samples_values = estimatorSamplesValues[estimator];
    size_t index = estimatorIndices[estimator];
    double error = estimator_samples_values[index];
    double adjusted_value = adjusted_estimate(estimate, error);

    return adjusted_value;
}

void
IntNWJointDistribution::processObservation(Estimator *estimator, double observation,
                                           double old_estimate, double new_estimate)
{
    if (estimate_is_valid(old_estimate) && estimatorSamples.count(estimator) > 0) {
        // if there's a prior estimate, we can calculate an error sample
        double error = calculate_error(old_estimate, observation);
        estimatorSamples[estimator]->addValue(error);
        
        inst::dbgprintf(INFO, "IntNWJoint: Added error observation to estimator %s: %f\n", 
                        estimator->getName().c_str(), error);
    } else if (estimatorSamples.count(estimator) == 0) {
        ensureSamplesDistributionExists(estimator);
    }
    clearEstimatorSamplesDistributions();
}

void
IntNWJointDistribution::processEstimatorConditionsChange(Estimator *estimator)
{
    clearEstimatorSamplesDistributions();
}

void 
IntNWJointDistribution::processEstimatorReset(Estimator *estimator, const char *filename)
{
    clearEstimatorSamplesDistributions();
    if (filename) {
        ifstream in(filename);
        if (!in) {
            ostringstream oss;
            oss << "Failed to open " << filename;
            throw runtime_error(oss.str());
        }
        
        restoreFromFile(in, estimator->getName());
    } else {
        if (estimatorSamples.count(estimator) > 0) {
            delete estimatorSamples[estimator];
            estimatorSamples.erase(estimator);
        }
        // default no-error value will be added later before it's actually used
    }
}

void
IntNWJointDistribution::saveToFile(ofstream& out)
{
    try {
        out << estimatorSamples.size() << " estimators" << endl;
        for (EstimatorSamplesMap::iterator it = estimatorSamples.begin();
             it != estimatorSamples.end(); ++it) {
            Estimator *estimator = it->first;
            StatsDistribution *dist = it->second;
            
            dist->appendToFile(estimator->getName(), out);
        }
    } catch (runtime_error& e) {
        inst::dbgprintf(ERROR, "WARNING: failed to save joint distribution: %s\n", e.what());
    }
}

struct MatchName {
    const string& key;
    Estimator *estimator;
    MatchName(const string& key_) : key(key_), estimator(NULL) {}
    bool operator()(const EstimatorSamplesMap::value_type& value) {
        if (value.first->getName() == key) {
            assert(estimator == NULL); // should only be one; it's a map
            estimator = value.first;
            return true;
        }
        return false;
    }
};

Estimator *
IntNWJointDistribution::getExistingEstimator(const string& key)
{
    if (estimatorSamples.size() == 0) { 
        return NULL;
    }
    EstimatorSamplesMap::iterator it = 
        find_if(estimatorSamples.begin(), estimatorSamples.end(), MatchName(key));
    if (it == estimatorSamples.end()) {
        return NULL;
    }
    return it->first;
}

void 
IntNWJointDistribution::restoreFromFile(ifstream& in)
{
    restoreFromFile(in, "");
}

void 
IntNWJointDistribution::restoreFromFile(ifstream& in, const string& estimator_name)
{
    try {
        size_t num_estimators = 0;
        string dummy;
        check(in >> num_estimators >> dummy, "Parse failure");
        
        for (size_t i = 0; i < num_estimators; ++i) {
            string key;
            StatsDistribution *dist = createSamplesDistribution();
            key = dist->restoreFromFile(in);
            
            if (estimator_name.empty() || estimator_name == key) {
                Estimator *estimator = getExistingEstimator(key);
                if (estimator) {
                    assert(estimatorSamples.count(estimator) > 0);
                    delete estimatorSamples[estimator];
                    estimatorSamples[estimator] = dist;
                } else {
                    estimatorSamplesPlaceholders[key] = dist;
                }
            } else {
                delete dist;
            }
        }

        clearEstimatorSamplesDistributions();
    } catch (runtime_error& e) {
        dbgprintf(ERROR, "WARNING: failed to restore joint distribution: %s\n", e.what());
    }
}
