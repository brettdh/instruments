#include "intnw_joint_distribution.h"

#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "estimator.h"
#include "error_calculation.h"
#include "debug.h"
namespace inst = instruments;
using inst::ERROR; using inst::INFO;

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

static const size_t NUM_ESTIMATORS_SINGULAR = 2;
static const size_t REDUNDANT_STRATEGY_CHILDREN = 2;
static const size_t NUM_ESTIMATORS_REDUNDANT = 
    NUM_ESTIMATORS_SINGULAR * REDUNDANT_STRATEGY_CHILDREN;

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

static void destroy_array(double *array)
{
    delete [] array;
}

static void destroy_array(double **array, size_t dim1)
{
    for (size_t i = 0; i < dim1; ++i) {
        destroy_array(array[i]);
    }
    delete [] array;
}

static double *get_estimator_values(StatsDistribution *estimator_samples, 
                                    double (StatsDistribution::Iterator::*fn)(int))
{
    StatsDistribution::Iterator *it = estimator_samples->getIterator();
    size_t len = it->totalCount();
    double *values = new double[len];
    for (size_t i = 0; i < len; ++i) {
        values[i] = (it->*fn)(i);
    }
    
    estimator_samples->finishIterator(it);
    return values;
}

static double *get_estimator_samples_values(StatsDistribution *estimator_samples)
{
    return get_estimator_values(estimator_samples, &StatsDistribution::Iterator::at);
}

static double *get_estimator_samples_probs(StatsDistribution *estimator_samples)
{
    return get_estimator_values(estimator_samples, &StatsDistribution::Iterator::probability);
}

static size_t get_estimator_samples_count(StatsDistribution *estimator_samples)
{
    StatsDistribution::Iterator *it = estimator_samples->getIterator();
    size_t len = it->totalCount();
    estimator_samples->finishIterator(it);
    return len;
}

IntNWJointDistribution::IntNWJointDistribution(StatsDistributionType dist_type,
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
    singular_strategy_saved_values = new double***[singular_strategy_estimators.size()];
    
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        singular_probabilities[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_samples_values[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_samples_count[i] = new size_t[NUM_ESTIMATORS_SINGULAR];
        singular_strategy_saved_values[i] = new double**[NUM_SAVED_VALUE_TYPES];
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            singular_strategy_saved_values[i][j] = NULL;
        }
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            singular_probabilities[i][j] = NULL;
            singular_samples_values[i][j] = NULL;
            singular_samples_count[i][j] = 0;
        }
    }
}

IntNWJointDistribution::~IntNWJointDistribution()
{
    clearEstimatorSamplesDistributions();
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        delete [] singular_probabilities[i];
        delete [] singular_samples_values[i];
        delete [] singular_samples_count[i];
        delete [] singular_strategy_saved_values[i];
    }
    delete [] singular_probabilities;
    delete [] singular_samples_values;
    delete [] singular_samples_count;
    delete [] singular_strategy_saved_values;

    for (EstimatorSamplesMap::iterator it = estimatorSamples.begin();
         it != estimatorSamples.end(); ++it) {
        delete it->second;
    }
    estimatorSamples.clear();
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
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] != NULL) {
            return;
        }

        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            ensureSamplesDistributionExists(estimator);
            StatsDistribution *distribution = estimatorSamples[estimator];
            ASSERT(distribution != NULL);
            singular_probabilities[i][j] = get_estimator_samples_probs(distribution);
            singular_samples_values[i][j] = get_estimator_samples_values(distribution);
            singular_samples_count[i][j] = get_estimator_samples_count(distribution);
        }
    }

    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            estimatorSamplesValues[estimator] = singular_samples_values[i][j];
            estimatorIndices[estimator] = 0;
        }
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            singular_strategy_saved_values[i][j] = create_array(singular_samples_count[i][0],
                                                                singular_samples_count[i][1],
                                                                DBL_MAX);
        }
    }
}

void 
IntNWJointDistribution::clearEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] == NULL) {
            return;
        }
        
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            destroy_array(singular_strategy_saved_values[i][j], singular_samples_count[i][0]);
            singular_strategy_saved_values[i][j] = NULL;
        }
        
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
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

    size_t saved_value_type = get_value_type(strategy, fn);

    
    double **cur_strategy_memo = NULL;
    double **strategy_probabilities = NULL;
    size_t *samples_count = NULL;
    vector<Estimator *> current_strategy_estimators;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        if (strategy == singular_strategies[i]) {
            cur_strategy_memo = singular_strategy_saved_values[i][saved_value_type];
            current_strategy_estimators = singular_strategy_estimators[i];
            samples_count = singular_samples_count[i];
            strategy_probabilities = singular_probabilities[i];
            break;
        }
    }
    assert(samples_count);
    assert(cur_strategy_memo);

    size_t max_i, max_j;
    max_i = samples_count[0];
    max_j = samples_count[1];

    double weightedSum = 0.0;
    for (size_t i = 0; i < max_i; ++i) {
        estimatorIndices[current_strategy_estimators[0]] = i;
        for (size_t j = 0; j < max_j; ++j) {
            estimatorIndices[current_strategy_estimators[1]] = j;
            double value = fn(this, strategy_arg, chooser_arg);
            double probability = getSingularJointProbability(strategy_probabilities,
                                                             i, j);
            cur_strategy_memo[i][j] = value;
            weightedSum += (value * probability);
        }
    }
    return weightedSum;
}

inline double
IntNWJointDistribution::getSingularJointProbability(double **strategy_probabilities,
                                                    size_t index0, size_t index1)
{
    return strategy_probabilities[0][index0] * strategy_probabilities[1][index1];
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
ensure_values_valid(double **saved_values, size_t max_i, size_t max_j, 
                    typesafe_eval_fn_t fn)
{
    for (size_t i = 0; i < max_i; ++i) {
        for (size_t j = 0; j < max_j; ++j) {
            assert(saved_values[i][j] != DBL_MAX || fn == NULL);
            ensure_valid(saved_values[i][j]);
        }
    }
}

void
IntNWJointDistribution::ensureValidMemoizedValues(eval_fn_type_t saved_value_type)
{
    size_t max_i, max_j, max_k, max_m;  /* counts for: */
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */
    max_j = singular_samples_count[0][1]; /* (strategy 0, estimator 1) */
    max_k = singular_samples_count[1][0]; /* (strategy 1, estimator 2) */
    max_m = singular_samples_count[1][1]; /* (strategy 1, estimator 3) */

    double **strategy_0_saved_values = singular_strategy_saved_values[0][saved_value_type];
    double **strategy_1_saved_values = singular_strategy_saved_values[1][saved_value_type];

    ensure_values_valid(strategy_0_saved_values, max_i, max_j,
                        singular_strategies[0]->getEvalFn(saved_value_type));
    ensure_values_valid(strategy_1_saved_values, max_k, max_m,
                        singular_strategies[1]->getEvalFn(saved_value_type));
}

double 
IntNWJointDistribution::redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
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


// indices are 4-way loop indices.
static inline double noop_joint_probability(size_t i, size_t j, size_t k, size_t m)
{
    // the joint probability is already calculated in the product of the
    //  singular probabilities.
    return 1.0;
}

#include "tight_loop.h"

double
IntNWJointDistribution::redundantStrategyExpectedValueMin(size_t saved_value_type)
{
    double weightedSum = 0.0;
    FN_BODY_WITH_COMBINER(weightedSum,
                          min,
                          get_one_redundant_probability, 
                          noop_joint_probability,
                          saved_value_type);
    return weightedSum;
}

double
IntNWJointDistribution::redundantStrategyExpectedValueSum(size_t saved_value_type)
{
    double weightedSum = 0.0;
    FN_BODY_WITH_COMBINER(weightedSum, 
                          sum,
                          get_one_redundant_probability, 
                          noop_joint_probability, 
                          saved_value_type);
    return weightedSum;
}


double
IntNWJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    double *estimator_samples_values = estimatorSamplesValues[estimator];
    size_t index = estimatorIndices[estimator];
    double estimate = estimator->getEstimate();
    double error = estimator_samples_values[index];
    double adjusted_value = adjusted_estimate(estimate, error);
    return adjusted_value;
}

void
IntNWJointDistribution::observationAdded(Estimator *estimator, double observation,
                                         double old_estimate, double new_estimate)
{
    if (estimate_is_valid(old_estimate) && estimatorSamples.count(estimator) > 0) {
        // if there's a prior estimate, we can calculate an error sample
        double error = calculate_error(old_estimate, observation);
        estimatorSamples[estimator]->addValue(error);
        
        inst::dbgprintf(INFO, "IntNWJoint: Added error observation to estimator %p: %f\n", estimator, error);
    } else if (estimatorSamples.count(estimator) == 0) {
        ensureSamplesDistributionExists(estimator);
    }
    clearEstimatorSamplesDistributions();
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
