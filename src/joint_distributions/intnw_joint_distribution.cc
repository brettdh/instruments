#include "intnw_joint_distribution.h"

#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "estimator.h"

#include <vector>
using std::vector;

static const size_t NUM_ESTIMATORS_SINGULAR = 2;
static const size_t REDUNDANT_STRATEGY_CHILDREN = 2;
static const size_t NUM_ESTIMATORS_REDUNDANT = 
    NUM_ESTIMATORS_SINGULAR * REDUNDANT_STRATEGY_CHILDREN;

static const size_t NUM_STRAEGIES = 3;


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

IntNWJointDistribution::IntNWJointDistribution(const std::vector<Strategy *>& strategies)
{
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
    singular_strategy_saved_values = new double**[singular_strategy_estimators.size()];
    
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        singular_probabilities[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_error_values[i] = new double*[NUM_ESTIMATORS_SINGULAR];
        singular_error_count[i] = new size_t[NUM_ESTIMATORS_SINGULAR];
        singular_strategy_saved_values[i] = NULL;
        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            singular_probabilities[i][j] = NULL;
            singular_error_values[i][j] = NULL;
            singular_error_count[i][j] = 0;
        }
    }
}

void
IntNWJointDistribution::getEstimatorErrorDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] != NULL) {
            break;
        }

        for (size_t j = 0; j < NUM_ESTIMATORS_SINGULAR; ++j) {
            Estimator *estimator = singular_strategy_estimators[i][j];
            StatsDistribution *distribution = estimatorError[estimator];
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
        singular_strategy_saved_values[i] = create_array(singular_error_count[i][0],
                                                         singular_error_count[i][1],
                                                         0.0);
    }
}

void 
IntNWJointDistribution::clearEstimatorErrorDistributions()
{
    for (size_t i = 0; i < singular_strategy_estimators.size(); ++i) {
        if (singular_probabilities[i][0] == NULL) {
            break;
        }
        
        destroy_array(singular_strategy_saved_values[i], singular_error_count[i][0]);
        singular_strategy_saved_values[i] = NULL;
        
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
    }

    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
IntNWJointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    if (strategy->isRedundant()) {
        assert(strategy->childrenAreDisjoint());
        return redundantStrategyExpectedValue(strategy, fn);
    } else {
        return singularStrategyExpectedValue(strategy, fn);
    }
}

double 
IntNWJointDistribution::singularStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    double ***saved_values = NULL;
    double **strategy_probabilities = NULL;
    size_t *error_count = NULL;
    vector<Estimator *> current_strategy_estimators;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        if (strategy == singular_strategies[i]) {
            saved_values = &singular_strategy_saved_values[i];
            current_strategy_estimators = singular_strategy_estimators[i];
            error_count = singular_error_count[i];
            strategy_probabilities = singular_probabilities[i];
            break;
        }
    }
    assert(saved_values);
    assert(error_count);
    
    size_t max_i, max_j;
    max_i = error_count[0];
    max_j = error_count[1];

    double **cur_strategy_memo = *saved_values;
    assert(cur_strategy_memo);

    getEstimatorErrorDistributions();

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

double 
IntNWJointDistribution::redundantStrategyExpectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        assert(singular_strategies_saved_valies[i] != NULL);
    }
    
    double weightedSum = 0.0;
    double (*combine)(double, double) = NULL;

    size_t max_i, max_j, max_k, max_m;  // counts for:
    max_i = singular_error_count[0][0]; // (strategy 0, estimator 0)
    max_j = singular_error_count[0][1]; // (strategy 0, estimator 1)
    max_k = singular_error_count[1][0]; // (strategy 1, estimator 2)
    max_m = singular_error_count[1][1]; // (strategy 1, estimator 3)

    for (size_t i = 0; i < max_i; ++i) {
        for (size_t j = 0; j < max_j; ++j) {
            for (size_t k = 0; i < max_k; ++k) {
                for (size_t m = 0; j < max_m; ++m) {
                    double value = combine(singular_strategy_saved_values[0][i][j], 
                                           singular_strategy_saved_values[1][k][m]);
                    double probability = 
                        singular_probabilities[0][0][i] * // strategy 0, estimator 0
                        singular_probabilities[0][1][j] * // strategy 0, estimator 1
                        singular_probabilities[1][0][k] * // strategy 1, estimator 0
                        singular_probabilities[0][1][m];  // strategy 1, estimator 1
                    weightedSum += (value * probability);
                }
            }
        }
    }
    return weightedSum;
}

double
IntNWJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    double *estimator_error_values = estimatorErrorValues[estimator];
    size_t index = estimatorIndices[estimator];
    return estimator->getEstimate() - estimator_error_values[index];
}

void
IntNWJointDistribution::observationAdded(Estimator *estimator, double value)
{
    if (estimatorError.count(estimator) > 0) {
        double error = estimator->getEstimate() - value;
        estimatorError[estimator]->addValue(error);
    } else {
        estimatorError[estimator] = createErrorDistribution();
        
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        estimatorError[estimator]->addValue(0.0);
    }
    clearEstimatorErrorDistributions();
}

StatsDistribution *
IntNWJointDistribution::createErrorDistribution()
{
    // make this an actual factory method, with a flag set in 
    //  the JointDistribution constructor.
    return new StatsDistributionAllSamples;
    //return new StatsDistributionBinned;
}
