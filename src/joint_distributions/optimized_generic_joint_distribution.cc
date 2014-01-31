#include "optimized_generic_joint_distribution.h"

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

static vector<double> get_estimator_values(StatsDistribution *estimator_samples, 
                                           double (StatsDistribution::Iterator::*fn)(size_t), size_t& count)
{
    StatsDistribution::Iterator *it = estimator_samples->getIterator();
    count = it->totalCount();
    vector<double> values;
    values.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        double value = (it->*fn)(i);
        values.push_back(value);
    }
    
    estimator_samples->finishIterator(it);
    return values;
}

static vector<double> get_estimator_samples_values(StatsDistribution *estimator_samples, size_t& count)
{
    return get_estimator_values(estimator_samples, &StatsDistribution::Iterator::at, count);
}

static vector<double> get_estimator_samples_probs(StatsDistribution *estimator_samples, size_t& count)
{
    return get_estimator_values(estimator_samples, &StatsDistribution::Iterator::probability, count);
}

static void adjust_probs_for_estimator_conditions(Estimator *estimator, vector<double>& values, vector<double>& probs, size_t& count)
{
    size_t index = count;
    size_t pruned_samples = 0;

    if (!estimator->hasConditions()) {
        return;
    }
    
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

OptimizedGenericJointDistribution::OptimizedGenericJointDistribution(StatsDistributionType dist_type,
                                                          const std::vector<Strategy *>& strategies_)
    : AbstractJointDistribution(dist_type)
{
    strategy_arg = NULL;
    chooser_arg = NULL;
    strategies = strategies_;

    for (Strategy *strategy : strategies_) {
        auto estimators = strategy->getEstimators();
        strategy_estimators.push_back(estimators);
        
        size_t num_estimators = estimators.size();
        loops.emplace_back(getNestedLoop(num_estimators));

        // example of these nested vector structures:
        // probabilities[strategy_index][estimator_index] = [vector of probability samples]
        probabilities.emplace_back(num_estimators, vector<double>());
        samples_values.emplace_back(num_estimators, vector<double>());
    }
    
}

OptimizedGenericJointDistribution::~OptimizedGenericJointDistribution()
{
}

void
OptimizedGenericJointDistribution::ensureSamplesDistributionExists(Estimator *estimator)
{
    string key = estimator->getName();
    if (estimatorSamplesPlaceholders.count(key) > 0) {
        estimatorSamples[estimator] = estimatorSamplesPlaceholders[key];
        estimatorSamplesPlaceholders.erase(key);
    } else if (estimatorSamples.count(estimator) == 0) {
        estimatorSamples[estimator] = createSamplesDistribution(estimator);
        addDefaultValue(estimator);
   }
    
    assert(estimatorSamples.count(estimator) > 0);
}

void
OptimizedGenericJointDistribution::addDefaultValue(Estimator *estimator)
{
    // don't add a real error value to the distribution.
    // there's no error until we have at least two observations.
    estimatorSamples[estimator]->addValue(no_error_value());
}

void
OptimizedGenericJointDistribution::getEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        if (!probabilities[i][0].empty()) {
            return;
        }

        size_t num_estimators = strategies[i]->getEstimators().size();
        for (size_t j = 0; j < num_estimators; ++j) {
            Estimator *estimator = strategy_estimators[i][j];
            if (!strategies[i]->usesEstimator(estimator)) {
                continue;
            }
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

        }
    }

    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        size_t num_estimators = strategies[i]->getEstimators().size();
        for (size_t j = 0; j < num_estimators; ++j) {
            Estimator *estimator = strategy_estimators[i][j];
            estimatorSamplesValues[estimator] = &samples_values[i][j];
            estimatorIndices[estimator] = 0;
        }
    }
}

void 
OptimizedGenericJointDistribution::clearEstimatorSamplesDistributions()
{
    for (size_t i = 0; i < strategy_estimators.size(); ++i) {
        if (probabilities[i][0].empty()) {
            return;
        }
        
        size_t num_estimators = strategies[i]->getEstimators().size();
        for (size_t j = 0; j < num_estimators; ++j) {
            probabilities[i][j].clear();
            samples_values[i][j].clear();
        }
    }
    estimatorSamplesValues.clear();
    estimatorIndices.clear();
}

void 
OptimizedGenericJointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearEstimatorSamplesDistributions();
    }

    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
OptimizedGenericJointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    getEstimatorSamplesDistributions();
    
    size_t strategy_index = strategies.size();
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (strategy == strategies[i]) {
            strategy_index = i;
            break;
        }
    }
    assert(strategy_index < strategies.size());
    
    vector<vector<double> >& cur_strategy_probabilities = probabilities[strategy_index];
    //vector<vector<double> >& cur_strategy_values = samples_values[strategy_index];
    vector<Estimator *>& cur_strategy_estimators = strategy_estimators[strategy_index];
    
    vector<size_t> loop_dims;
    for (vector<double>& probs : cur_strategy_probabilities) {
        loop_dims.push_back(probs.size());
    }
    
    double weightedSum = 0.0;
    
    if (inst::is_debugging_on(DEBUG)) {
        inst::dbgprintf(DEBUG, "strategy \"%s\" (fn %s) uses %zu estimators\n", 
                        strategy->getName(), get_value_name(strategy, fn).c_str(), 
                        cur_strategy_estimators.size());
        for (size_t i = 0; i < cur_strategy_estimators.size(); ++i) {
            Estimator *estimator = cur_strategy_estimators[i];
            ostringstream s;
            s << "  " << estimator->getName() << ": ";
            for (size_t j = 0; j < loop_dims[i]; ++j) {
                estimatorIndices[estimator] = j;
                s << getAdjustedEstimatorValue(estimator) << " ";
            }
            inst::dbgprintf(DEBUG, "%s\n", s.str().c_str());
        }
    }

    ostringstream indices_values;
    ostringstream estimator_values;

    AbstractNestedLoop::Looper loop_body = [&](vector<size_t>& indices) {
        if (inst::is_debugging_on(DEBUG)) {
            indices_values.str("");
            estimator_values.str("");
        }
        
        double probability = 1.0;
        for (size_t i = 0; i < indices.size(); ++i) {
            Estimator *estimator = cur_strategy_estimators[i];
            estimatorIndices[estimator] = indices[i];
            probability *= cur_strategy_probabilities[i][indices[i]];
            if (inst::is_debugging_on(DEBUG)) {
                indices_values << indices[i] << " ";
                estimator_values << getAdjustedEstimatorValue(estimator) << " ";
            }
        }
        double value = fn(this, strategy_arg, chooser_arg);
        weightedSum += value * probability;
        inst::dbgprintf(DEBUG, "  [ %s] [ %s]  value = %f  prob = %f  weightedSum = %f\n",
                        indices_values.str().c_str(),
                        estimator_values.str().c_str(),
                        value, probability, weightedSum);
    };

    ostringstream indices_max;
    if (inst::is_debugging_on(DEBUG)) {
        for (auto dim : loop_dims) {
            indices_max << dim << " ";
        }
    }
    
    inst::dbgprintf(DEBUG, "About to run %zu-way nested loop, dims [ %s]\n", 
                    loop_dims.size(), indices_max.str().c_str());
    auto& loop = loops[strategy_index];
    loop->set_dims(loop_dims);
    loop->run_loop(loop_body);
    
    return weightedSum;
}

double
OptimizedGenericJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    double estimate = estimator->getEstimate();
    if (estimatorSamplesValues.count(estimator) == 0) {
        return estimate;
    }
    vector<double> *estimator_samples_values = estimatorSamplesValues[estimator];
    size_t index = estimatorIndices[estimator];
    double error = (*estimator_samples_values)[index];
    double adjusted_value = adjusted_estimate(estimate, error);

    return adjusted_value;
}

void
OptimizedGenericJointDistribution::processObservation(Estimator *estimator, double observation,
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
OptimizedGenericJointDistribution::processEstimatorConditionsChange(Estimator *estimator)
{
    clearEstimatorSamplesDistributions();
}

void 
OptimizedGenericJointDistribution::processEstimatorReset(Estimator *estimator, const char *filename)
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
OptimizedGenericJointDistribution::saveToFile(ofstream& out)
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
    bool operator()(const OptimizedGenericJointDistribution::EstimatorSamplesMap::value_type& value) {
        if (value.first->getName() == key) {
            assert(estimator == NULL); // should only be one; it's a map
            estimator = value.first;
            return true;
        }
        return false;
    }
};

Estimator *
OptimizedGenericJointDistribution::getExistingEstimator(const string& key)
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
OptimizedGenericJointDistribution::restoreFromFile(ifstream& in)
{
    restoreFromFile(in, "");
}

void 
OptimizedGenericJointDistribution::restoreFromFile(ifstream& in, const string& estimator_name)
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
