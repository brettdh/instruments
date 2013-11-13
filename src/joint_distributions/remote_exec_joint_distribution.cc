#include "remote_exec_joint_distribution.h"

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

static const size_t LOCAL_STRATEGY_INDEX = 0;
static const size_t REMOTE_STRATEGY_INDEX = 1;

static size_t NUM_ESTIMATORS_SINGULAR[] = {1, 3};

static const size_t REDUNDANT_STRATEGY_CHILDREN = 2;

static const size_t NUM_STRATEGIES = 3;

static const size_t NUM_SAVED_VALUE_TYPES = DATA_FN + 1;

static double *create_array(size_t length, double value)
{
    double *array = new double[length];
    for (size_t i = 0; i < length; ++i) {
        array[i] = value;
    }
    return array;
}

static double **create_array(size_t dim1, size_t dim2, double value)
{
    double **array = new double*[dim1];
    for (size_t i = 0; i < dim1; ++i) {
        array[i] = create_array(dim2, value);
    }
    return array;
}

static double ***create_array(size_t dim1, size_t dim2, size_t dim3, double value)
{
    double ***array = new double**[dim1];
    for (size_t i = 0; i < dim1; ++i) {
        array[i] = create_array(dim2, dim3, value);
    }
    return array;
}

static void destroy_array(double *array)
{
    delete [] array;
}

static void destroy_array(double **array, size_t dim1)
{
    if (array) {
        for (size_t i = 0; i < dim1; ++i) {
            destroy_array(array[i]);
        }
    }
    delete [] array;
}

static void destroy_array(double ***array, size_t dim1, size_t dim2)
{
    if (array) {
        for (size_t i = 0; i < dim1; ++i) {
            destroy_array(array[i], dim2);
        }
    }
    delete [] array;
}

static double *get_estimator_values(StatsDistribution *estimator_samples, 
                                    double (StatsDistribution::Iterator::*fn)(size_t), size_t& count)
{
    StatsDistribution::Iterator *it = estimator_samples->getIterator();
    count = it->totalCount();
    double *values = new double[count];
    for (size_t i = 0; i < count; ++i) {
        values[i] = (it->*fn)(i);
    }
    
    estimator_samples->finishIterator(it);
    return values;
}

static double *get_estimator_samples_values(StatsDistribution *estimator_samples, size_t& count)
{
    return get_estimator_values(estimator_samples, &StatsDistribution::Iterator::at, count);
}

static double *get_estimator_samples_probs(StatsDistribution *estimator_samples, size_t& count)
{
    double *values = get_estimator_values(estimator_samples, &StatsDistribution::Iterator::probability, count);
    return values;
}

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

RemoteExecJointDistribution::RemoteExecJointDistribution(StatsDistributionType dist_type,
                                               const std::vector<Strategy *>& strategies)
    : AbstractJointDistribution(dist_type)
{
    strategy_arg = NULL;
    chooser_arg = NULL;

    assert(strategies.size() == NUM_STRATEGIES);
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (!strategies[i]->isRedundant()) {
            singular_strategies.push_back(strategies[i]);
            singular_strategy_estimators.push_back(strategies[i]->getEstimators());
        }
    }
    assert(singular_strategies.size() == REDUNDANT_STRATEGY_CHILDREN);
    
    singular_probabilities = new double**[singular_strategy_estimators.size()];
    singular_samples_values = new double**[singular_strategy_estimators.size()];
    singular_samples_count = new size_t*[singular_strategy_estimators.size()];
    local_strategy_saved_values = new double*[NUM_SAVED_VALUE_TYPES];
    remote_strategy_saved_values = new double***[NUM_SAVED_VALUE_TYPES];
    for (size_t i = 0; i < NUM_SAVED_VALUE_TYPES; ++i) {
        local_strategy_saved_values[i] = NULL;
        remote_strategy_saved_values[i] = NULL;
    }
    
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        singular_probabilities[i] = new double*[NUM_ESTIMATORS_SINGULAR[i]];
        singular_samples_values[i] = new double*[NUM_ESTIMATORS_SINGULAR[i]];
        singular_samples_count[i] = new size_t[NUM_ESTIMATORS_SINGULAR[i]];
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            singular_probabilities[i][j] = NULL;
            singular_samples_values[i][j] = NULL;
            singular_samples_count[i][j] = 0;
        }
    }
}

RemoteExecJointDistribution::~RemoteExecJointDistribution()
{
    clearEstimatorSamplesDistributions();
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        delete [] singular_probabilities[i];
        delete [] singular_samples_values[i];
        delete [] singular_samples_count[i];
    }
    delete [] singular_probabilities;
    delete [] singular_samples_values;
    delete [] singular_samples_count;
    delete [] local_strategy_saved_values;
    delete [] remote_strategy_saved_values;

    for (EstimatorSamplesMap::iterator it = estimatorSamples.begin();
         it != estimatorSamples.end(); ++it) {
        delete it->second;
    }
    estimatorSamples.clear();
}

void
RemoteExecJointDistribution::ensureSamplesDistributionExists(Estimator *estimator)
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
RemoteExecJointDistribution::addDefaultValue(Estimator *estimator)
{
    // don't add a real error value to the distribution.
    // there's no error until we have at least two observations.
    estimatorSamples[estimator]->addValue(no_error_value());
}

void
RemoteExecJointDistribution::getEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] != NULL) {
            return;
        }

        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            ensureSamplesDistributionExists(estimator);
            StatsDistribution *distribution = estimatorSamples[estimator];
            ASSERT(distribution != NULL);
            size_t count = 0, count_probs = 42;
            singular_probabilities[i][j] = get_estimator_samples_probs(distribution, count_probs);
            singular_samples_values[i][j] = get_estimator_samples_values(distribution, count);
            assert(count == count_probs);

            adjust_probs_for_estimator_conditions(estimator, 
                                                  singular_samples_values[i][j],
                                                  singular_probabilities[i][j], count);

            singular_samples_count[i][j] = count;
        }
    }

    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            estimatorSamplesValues[estimator] = singular_samples_values[i][j];
            estimatorIndices[estimator] = 0;
        }
    }
    for (size_t i = 0; i < NUM_SAVED_VALUE_TYPES; ++i) {
        local_strategy_saved_values[i] = create_array(singular_samples_count[LOCAL_STRATEGY_INDEX][0],
                                                      DBL_MAX);
        remote_strategy_saved_values[i] = create_array(singular_samples_count[REMOTE_STRATEGY_INDEX][0],
                                                       singular_samples_count[REMOTE_STRATEGY_INDEX][1],
                                                       singular_samples_count[REMOTE_STRATEGY_INDEX][2],
                                                       DBL_MAX);
    }
}

void 
RemoteExecJointDistribution::clearEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < NUM_SAVED_VALUE_TYPES; ++i) {
        destroy_array(local_strategy_saved_values[i]);
        destroy_array(remote_strategy_saved_values[i], 
                      singular_samples_count[REMOTE_STRATEGY_INDEX][0],
                      singular_samples_count[REMOTE_STRATEGY_INDEX][1]);
        local_strategy_saved_values[i] = NULL;
        remote_strategy_saved_values[i] = NULL;
    }
    
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] == NULL) {
            return;
        }
        
        
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR[i]; ++j) {
            delete [] singular_probabilities[i][j];
            delete [] singular_samples_values[i][j];
            singular_probabilities[i][j] = NULL;
            singular_samples_values[i][j] = NULL;
            
            singular_samples_count[i][j] = 0;
        }
    }
    estimatorSamplesValues.clear();
    estimatorIndices.clear();
    cache.clear();
}

void 
RemoteExecJointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearEstimatorSamplesDistributions();
    }

    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
RemoteExecJointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
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
RemoteExecJointDistribution::singularStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    getEstimatorSamplesDistributions();

    size_t saved_value_type = get_value_type(strategy, fn);

    bool remote = false;
    double **strategy_probabilities = NULL;
    size_t *samples_count = NULL;
    vector<Estimator *> current_strategy_estimators;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        if (strategy == singular_strategies[i]) {
            current_strategy_estimators = singular_strategy_estimators[i];
            samples_count = singular_samples_count[i];
            strategy_probabilities = singular_probabilities[i];
            
            remote = (i == REMOTE_STRATEGY_INDEX);
            
            break;
        }
    }
    assert(samples_count);
    
    double weightedSum = 0.0;
    
    size_t max_i, max_j=0, max_k=0;
    max_i = samples_count[0];
    
    if (remote) {
        max_j = samples_count[1];
        max_k = samples_count[2];
    }

    if (inst::is_debugging_on(DEBUG)) {
        inst::dbgprintf(DEBUG, "strategy \"%s\" (fn %s) uses %zu estimators\n", 
                        strategy->getName(), get_value_name(strategy, fn).c_str(), 
                        current_strategy_estimators.size());
        const size_t sizes[] = { max_i, max_j, max_k };
        for (size_t m = 0; m < (remote ? 3 : 1); ++m) {
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

    for (size_t i = 0; i < max_i; ++i) {
        estimatorIndices[current_strategy_estimators[0]] = i;
        double probability = strategy_probabilities[0][i];
        if (remote) {
            for (size_t j = 0; j < max_j; ++j) {
                estimatorIndices[current_strategy_estimators[1]] = j;
                double j_probability = probability * strategy_probabilities[1][j];
                for (size_t k = 0; k < max_k; ++k) {
                    estimatorIndices[current_strategy_estimators[2]] = k;
                    double k_probability = j_probability * strategy_probabilities[2][k];
                    double value = fn(this, strategy_arg, chooser_arg);
                    remote_strategy_saved_values[saved_value_type][i][j][k] = value;
                    weightedSum += value * k_probability;
                }
            }
        } else {
            double value = fn(this, strategy_arg, chooser_arg);
            local_strategy_saved_values[saved_value_type][i] = value;
            weightedSum += (value * probability);
        }
    }
    return weightedSum;
}

static inline double
get_one_redundant_probability(double ***singular_probabilities,
                              size_t strategy_index,
                              size_t estimator_index,
                              size_t distribution_index)
{
    return singular_probabilities[strategy_index][estimator_index][distribution_index];
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

static void ensure_valid(double& memoized_value)
{
    if (memoized_value == DBL_MAX) {
        memoized_value = 0.0;
    }
}

static void
ensure_values_valid(double *saved_values, size_t max_i,
                    typesafe_eval_fn_t fn)
{
    for (size_t i = 0; i < max_i; ++i) {
        assert(saved_values[i] != DBL_MAX || fn == NULL);
        ensure_valid(saved_values[i]);
    }
}

static void
ensure_values_valid(double **saved_values, size_t max_i, size_t max_j, 
                    typesafe_eval_fn_t fn)
{
    for (size_t i = 0; i < max_i; ++i) {
        ensure_values_valid(saved_values[i], max_j, fn);
    }
}

void
RemoteExecJointDistribution::ensureValidMemoizedValues(eval_fn_type_t saved_value_type)
{
    size_t max_i, max_j, max_k, max_m;  /* counts for: */
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */
    max_j = singular_samples_count[1][0]; /* (strategy 1, estimator 1) */
    max_k = singular_samples_count[1][1]; /* (strategy 1, estimator 2) */
    max_m = singular_samples_count[1][2]; /* (strategy 1, estimator 3) */

    ensure_values_valid(local_strategy_saved_values[saved_value_type], max_m,
                        singular_strategies[LOCAL_STRATEGY_INDEX]->getEvalFn(saved_value_type));
    for (size_t i = 0; i < max_i; ++i) {
        ensure_values_valid(remote_strategy_saved_values[saved_value_type][i], max_j, max_k,
                            singular_strategies[REMOTE_STRATEGY_INDEX]->getEvalFn(saved_value_type));
    }
}

double 
RemoteExecJointDistribution::redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    eval_fn_type_t saved_value_type = get_value_type(strategy, fn);
    
    ensureValidMemoizedValues(saved_value_type);
    
    if (fn == redundant_strategy_minimum_time) {
        return redundantStrategyExpectedValueMin(saved_value_type);
    } else if (fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost) {
        return redundantStrategyExpectedValueSum(saved_value_type);
    } else abort();
}


#include "tight_loop_remote_exec.h"

double
RemoteExecJointDistribution::redundantStrategyExpectedValueMin(size_t saved_value_type)
{
    double weightedSum = 0.0;
    FN_BODY_WITH_COMBINER(weightedSum, min, saved_value_type);
    return weightedSum;
}

double
RemoteExecJointDistribution::redundantStrategyExpectedValueSum(size_t saved_value_type)
{
    double weightedSum = 0.0;
    FN_BODY_WITH_COMBINER(weightedSum, sum, saved_value_type);
    return weightedSum;
}


double
RemoteExecJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
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
RemoteExecJointDistribution::processObservation(Estimator *estimator, double observation,
                                           double old_estimate, double new_estimate)
{
    if (estimate_is_valid(old_estimate) && estimatorSamples.count(estimator) > 0) {
        // if there's a prior estimate, we can calculate an error sample
        double error = calculate_error(old_estimate, observation);
        estimatorSamples[estimator]->addValue(error);
        
        inst::dbgprintf(INFO, "RemoteExecJoint: Added error observation to estimator %s: %f\n", 
                        estimator->getName().c_str(), error);
    } else if (estimatorSamples.count(estimator) == 0) {
        ensureSamplesDistributionExists(estimator);
    }
    clearEstimatorSamplesDistributions();
}

void
RemoteExecJointDistribution::processEstimatorConditionsChange(Estimator *estimator)
{
    clearEstimatorSamplesDistributions();
}

void
RemoteExecJointDistribution::saveToFile(ofstream& out)
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
RemoteExecJointDistribution::getExistingEstimator(const string& key)
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
RemoteExecJointDistribution::restoreFromFile(ifstream& in)
{
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
        dbgprintf(ERROR, "WARNING: failed to restore joint distribution: %s\n", e.what());
    }
}
