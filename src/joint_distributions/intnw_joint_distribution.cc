#include "intnw_joint_distribution.h"

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
using std::vector; using std::ifstream; using std::ofstream; using std::count_if;
using std::ostringstream; using std::endl;
using std::runtime_error; using std::string;

static const size_t NUM_ESTIMATORS_SINGULAR = 2;
static const size_t REDUNDANT_STRATEGY_CHILDREN = 2;
static const size_t NUM_ESTIMATORS_REDUNDANT = 
    NUM_ESTIMATORS_SINGULAR * REDUNDANT_STRATEGY_CHILDREN;

static const size_t NUM_STRATEGIES = 3;

static const size_t NUM_SAVED_VALUE_TYPES = DATA_FN + 1;

static eval_fn_type_t
get_saved_value_type(Strategy *strategy, typesafe_eval_fn_t fn)
{
    if (fn == strategy->getEvalFn(TIME_FN)) {
        return TIME_FN;
    } else if (fn == strategy->getEvalFn(ENERGY_FN)) {
        return ENERGY_FN;
    } else if (fn == strategy->getEvalFn(DATA_FN)) {
        return DATA_FN;
    } else abort();
}

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

static double *get_estimator_values(StatsDistribution *estimator_error, 
                                    double (StatsDistribution::Iterator::*fn)(int))
{
    StatsDistribution::Iterator *it = estimator_error->getIterator();
    size_t len = it->totalCount();
    double *values = new double[len];
    for (size_t i = 0; i < len; ++i) {
        values[i] = (it->*fn)(i);
    }
    
    estimator_error->finishIterator(it);
    return values;
}

static double *get_estimator_error_values(StatsDistribution *estimator_error)
{
    return get_estimator_values(estimator_error, &StatsDistribution::Iterator::at);
}

static double *get_estimator_error_probs(StatsDistribution *estimator_error)
{
    return get_estimator_values(estimator_error, &StatsDistribution::Iterator::probability);
}

static size_t get_estimator_samples_count(StatsDistribution *estimator_error)
{
    StatsDistribution::Iterator *it = estimator_error->getIterator();
    size_t len = it->totalCount();
    estimator_error->finishIterator(it);
    return len;
}

IntNWJointDistribution::IntNWJointDistribution(EmpiricalErrorEvalMethod eval_method,
                                               const std::vector<Strategy *>& strategies)
    : AbstractJointDistribution(eval_method)
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
    singular_error_values = new double**[singular_strategy_estimators.size()];
    singular_error_count = new size_t*[singular_strategy_estimators.size()];
    singular_strategy_saved_values = new double***[singular_strategy_estimators.size()];
    
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        singular_probabilities[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_error_values[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_error_count[i] = new size_t[NUM_ESTIMATORS_SINGULAR];
        singular_strategy_saved_values[i] = new double**[NUM_SAVED_VALUE_TYPES];
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            singular_strategy_saved_values[i][j] = NULL;
        }
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            singular_probabilities[i][j] = NULL;
            singular_error_values[i][j] = NULL;
            singular_error_count[i][j] = 0;
        }
    }
}

IntNWJointDistribution::~IntNWJointDistribution()
{
    clearEstimatorErrorDistributions();
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        delete [] singular_probabilities[i];
        delete [] singular_error_values[i];
        delete [] singular_error_count[i];
        delete [] singular_strategy_saved_values[i];
    }
    delete [] singular_probabilities;
    delete [] singular_error_values;
    delete [] singular_error_count;
    delete [] singular_strategy_saved_values;
}

void
IntNWJointDistribution::ensureErrorDistributionExists(Estimator *estimator)
{
    string key = estimator->getName();
    if (estimatorErrorPlaceholders.count(key) > 0) {
        estimatorError[estimator] = estimatorErrorPlaceholders[key];
        estimatorErrorPlaceholders.erase(key);
    } else if (estimatorError.count(estimator) == 0) {
        estimatorError[estimator] = createErrorDistribution();
         
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        estimatorError[estimator]->addValue(no_error_value());
   }
    
    assert(estimatorError.count(estimator) > 0);
}

void
IntNWJointDistribution::getEstimatorErrorDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] != NULL) {
            return;
        }

        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            ensureErrorDistributionExists(estimator);
            StatsDistribution *distribution = estimatorError[estimator];
            ASSERT(distribution != NULL);
            singular_probabilities[i][j] = get_estimator_error_probs(distribution);
            singular_error_values[i][j] = get_estimator_error_values(distribution);
            singular_error_count[i][j] = get_estimator_samples_count(distribution);
        }
    }

    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            estimatorErrorValues[estimator] = singular_error_values[i][j];
            estimatorIndices[estimator] = 0;
        }
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            singular_strategy_saved_values[i][j] = create_array(singular_error_count[i][0],
                                                                singular_error_count[i][1],
                                                                DBL_MAX);
        }
    }
}

void 
IntNWJointDistribution::clearEstimatorErrorDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] == NULL) {
            return;
        }
        
        for (size_t j = 0; j < NUM_SAVED_VALUE_TYPES; ++j) {
            destroy_array(singular_strategy_saved_values[i][j], singular_error_count[i][0]);
            singular_strategy_saved_values[i][j] = NULL;
        }
        
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            delete [] singular_probabilities[i][j];
            delete [] singular_error_values[i][j];
            singular_probabilities[i][j] = NULL;
            singular_error_values[i][j] = NULL;
            
            singular_error_count[i][j] = 0;
        }
    }
    estimatorErrorValues.clear();
    estimatorIndices.clear();
}

void 
IntNWJointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearEstimatorErrorDistributions();
        cache.clear();
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
            assert(strategy->childrenAreDisjoint());
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
    getEstimatorErrorDistributions();

    size_t saved_value_type = get_saved_value_type(strategy, fn);

    
    double **cur_strategy_memo = NULL;
    double **strategy_probabilities = NULL;
    size_t *error_count = NULL;
    vector<Estimator *> current_strategy_estimators;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        if (strategy == singular_strategies[i]) {
            cur_strategy_memo = singular_strategy_saved_values[i][saved_value_type];
            current_strategy_estimators = singular_strategy_estimators[i];
            error_count = singular_error_count[i];
            strategy_probabilities = singular_probabilities[i];
            break;
        }
    }
    assert(error_count);
    assert(cur_strategy_memo);

    size_t max_i, max_j;
    max_i = error_count[0];
    max_j = error_count[1];

    double weightedSum = 0.0;
    for (size_t i = 0; i < max_i; ++i) {
        estimatorIndices[current_strategy_estimators[0]] = i;
        for (size_t j = 0; j < max_j; ++j) {
            estimatorIndices[current_strategy_estimators[1]] = j;
            double value = fn(this, strategy_arg, chooser_arg);
            double probability = strategy_probabilities[0][i] * strategy_probabilities[1][j];
            cur_strategy_memo[i][j] = value;
            weightedSum += (value * probability);
        }
    }
    return weightedSum;
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

typedef double (*combiner_fn_t)(double, double);

combiner_fn_t
combiner_fn(typesafe_eval_fn_t fn)
{
    if (fn == redundant_strategy_minimum_time) {
        return min;
    } else if (fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost) {
        return sum;
    } else abort();
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
    size_t max_i, max_j, max_k, max_m;  /* counts for: */                          \
    max_i = singular_error_count[0][0]; /* (strategy 0, estimator 0) */            \
    max_j = singular_error_count[0][1]; /* (strategy 0, estimator 1) */            \
    max_k = singular_error_count[1][0]; /* (strategy 1, estimator 2) */            \
    max_m = singular_error_count[1][1]; /* (strategy 1, estimator 3) */            \

    double **strategy_0_saved_values = singular_strategy_saved_values[0][saved_value_type];          \
    double **strategy_1_saved_values = singular_strategy_saved_values[1][saved_value_type];          \

    ensure_values_valid(strategy_0_saved_values, max_i, max_j,
                        singular_strategies[0]->getEvalFn(saved_value_type));
    ensure_values_valid(strategy_1_saved_values, max_k, max_m,
                        singular_strategies[1]->getEvalFn(saved_value_type));
}

double 
IntNWJointDistribution::redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    eval_fn_type_t saved_value_type = get_saved_value_type(strategy, fn);
    
    ensureValidMemoizedValues(saved_value_type);
    
    if (fn == redundant_strategy_minimum_time) {
        return redundantStrategyExpectedValueMin(saved_value_type);
    } else if (fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost) {
        return redundantStrategyExpectedValueSum(saved_value_type);
    } else abort();
}

#define FN_BODY_WITH_COMBINER(COMBINER, saved_value_type)                          \
{                                                                                  \
    for (size_t i = 0; i < singular_strategies.size(); ++i) {                      \
        assert(singular_strategy_saved_values[i] != NULL);                         \
        assert(singular_strategy_saved_values[i][saved_value_type] != NULL);      \
    }                                                                              \
                                                                                   \
    double weightedSum = 0.0;                                                      \
                                                                                   \
    size_t max_i, max_j, max_k, max_m;  /* counts for: */                          \
    max_i = singular_error_count[0][0]; /* (strategy 0, estimator 0) */            \
    max_j = singular_error_count[0][1]; /* (strategy 0, estimator 1) */            \
    max_k = singular_error_count[1][0]; /* (strategy 1, estimator 2) */            \
    max_m = singular_error_count[1][1]; /* (strategy 1, estimator 3) */            \
                                                                                   \
    double *strategy_0_estimator_0_probs = singular_probabilities[0][0];           \
    double *strategy_0_estimator_1_probs = singular_probabilities[0][1];           \
    double *strategy_1_estimator_0_probs = singular_probabilities[1][0];           \
    double *strategy_1_estimator_1_probs = singular_probabilities[1][1];           \
                                                                                   \
    double **strategy_0_saved_values = singular_strategy_saved_values[0][saved_value_type];          \
    double **strategy_1_saved_values = singular_strategy_saved_values[1][saved_value_type];          \
                                                                                   \
    for (size_t i = 0; i < max_i; ++i) {                                           \
        double *tmp_i = strategy_0_saved_values[i];                                \
        double prob_i = strategy_0_estimator_0_probs[i];                           \
        for (size_t j = 0; j < max_j; ++j) {                                       \
            double tmp_strategy_0 = tmp_i[j];                                      \
            assert(tmp_strategy_0 != DBL_MAX);                                     \
            double prob_j = prob_i * strategy_0_estimator_1_probs[j];              \
            for (size_t k = 0; k < max_k; ++k) {                                   \
                double *tmp_k = strategy_1_saved_values[k];                        \
                double prob_k = prob_j * strategy_1_estimator_0_probs[k];          \
                for (size_t m = 0; m < max_m; ++m) {                               \
                    double tmp_strategy_1 = tmp_k[m];                              \
                    assert(tmp_strategy_1 != DBL_MAX);                             \
                                                                                   \
                    double value = COMBINER(tmp_strategy_0, tmp_strategy_1);       \
                    double probability = prob_k * strategy_1_estimator_1_probs[m]; \
                    /*fprintf(stderr, "probs: [%.10f %.10f %.10f %.10f]\n", prob_i, prob_j, prob_k, probability);*/ \
                    weightedSum += (value * probability);                          \
                    /*fprintf(stderr, "strategy1 value = %.10f  strategy2 value = %.10f\n", tmp_strategy_0, tmp_k[m]);*/ \
                      /*fprintf(stderr, "value = %.10f prob = %.10f weightedSum = %.20f\n", value, probability, weightedSum);*/ \
                }                                                                  \
            }                                                                      \
        }                                                                          \
    }                                                                              \
    return weightedSum;                                                            \
}

double
IntNWJointDistribution::redundantStrategyExpectedValueMin(size_t saved_value_type)
{
    FN_BODY_WITH_COMBINER(min, saved_value_type);
}

double
IntNWJointDistribution::redundantStrategyExpectedValueSum(size_t saved_value_type)
{
    FN_BODY_WITH_COMBINER(sum, saved_value_type);
}


double
IntNWJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    double *estimator_error_values = estimatorErrorValues[estimator];
    size_t index = estimatorIndices[estimator];
    double estimate = estimator->getEstimate();
    double error = estimator_error_values[index];
    //double adjusted_value = estimate - error;
    double adjusted_value = adjusted_estimate(estimate, error);
    return adjusted_value;
}

void
IntNWJointDistribution::observationAdded(Estimator *estimator, double value)
{
    if (estimator->hasEstimate() && estimatorError.count(estimator) > 0) {
        // if there's a prior estimate, we can calculate an error sample
        double error = calculate_error(estimator->getEstimate(), value);
        estimatorError[estimator]->addValue(error);
        
        dbgprintf("IntNWJoint: Added error value to estimator %p: %f\n", estimator, error);
    } else if (estimatorError.count(estimator) == 0) {
        ensureErrorDistributionExists(estimator);
    }
    clearEstimatorErrorDistributions();
}

void
IntNWJointDistribution::saveToFile(ofstream& out)
{
    try {
        out << estimatorError.size() << " estimators" << endl;
        for (EstimatorErrorMap::iterator it = estimatorError.begin();
             it != estimatorError.end(); ++it) {
            Estimator *estimator = it->first;
            StatsDistribution *dist = it->second;
            
            dist->appendToFile(estimator->getName(), out);
        }
    } catch (runtime_error& e) {
        dbgprintf("WARNING: failed to save joint distribution: %s\n", e.what());
    }
}

struct MatchName {
    const string& key;
    MatchName(const string& key_) : key(key_) {}
    bool operator()(const EstimatorErrorMap::value_type& value) {
        return (value.first->getName() == key);
    }
};

bool
IntNWJointDistribution::estimatorExists(const string& key)
{
    if (estimatorError.size() == 0) { 
        return false;
    }
    return count_if(estimatorError.begin(), estimatorError.end(), MatchName(key));
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
            StatsDistribution *dist = createErrorDistribution();
            key = dist->restoreFromFile(in);
            
            if (estimatorExists(key)) {
                dbgprintf("WARNING: creating placholder %s for already-existing estimator\n",
                          key.c_str());
            }
            estimatorErrorPlaceholders[key] = dist;
        }

        clearEstimatorErrorDistributions();
    } catch (runtime_error& e) {
        dbgprintf("WARNING: failed to restore joint distribution: %s\n", e.what());
    }
}
