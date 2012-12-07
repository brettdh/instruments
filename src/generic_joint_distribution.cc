#include "generic_joint_distribution.h"
#include "strategy.h"
#include "estimator.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#ifndef ANDROID
#include "stats_distribution_binned.h"
#endif

#include "multi_dimension_array.h"

#include <assert.h>
#include <float.h>
#include <stdlib.h>

#include <functional>
#include <stdexcept>
#include <vector>
using std::vector; using std::min; using std::runtime_error;

GenericJointDistribution::GenericJointDistribution(EmpiricalErrorEvalMethod eval_method_,
                                     const vector<Strategy *>& strategies)
    : AbstractJointDistribution(eval_method_)
{
    for (size_t i = 0; i < strategies.size(); ++i) {
        Strategy *strategy = strategies[i];
        if (!strategy->isRedundant()) {
            singular_strategies.push_back(strategy);
        }
    }
    strategy_arg = NULL;
    chooser_arg = NULL;
    time_memos.resize(singular_strategies.size(), NULL);
    energy_memos.resize(singular_strategies.size(), NULL);
    data_memos.resize(singular_strategies.size(), NULL);
    iterator = NULL;
}

struct memoized_strategy_args {
    Strategy *parent_strategy;
    size_t num_strategies;
    typesafe_eval_fn_t real_fn;
    void *real_strategy_arg;
    vector<MultiDimensionArray<double> *> *memos;
    
    // only used for singular strategies.
    size_t strategy_index;
};

double
memoized_min_time(StrategyEvaluationContext *ctx, 
                  void *strategy_arg, void *chooser_arg)
{
    GenericJointDistribution *jointDistribution = (GenericJointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    vector<MultiDimensionArray<double> *>& memos = *args->memos;

    // valid because singular strategies are always evaluated before redundant strategies.
    double minValue = DBL_MAX;
    for (size_t i = 0; i < args->num_strategies; ++i) {
        double value = jointDistribution->getMemoizedValue(memos[i], i);
        assert(value != DBL_MAX);
        minValue = min(minValue, value);
    }
    return minValue;
}

double
memoized_total_energy_cost(StrategyEvaluationContext *ctx,
                           void *strategy_arg, void *chooser_arg)
{
    GenericJointDistribution *jointDistribution = (GenericJointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    vector<MultiDimensionArray<double> *>& memos = *args->memos;
    Strategy *parent = args->parent_strategy;
    vector<Strategy *> children = parent->getChildStrategies();
    assert(args->num_strategies == children.size());

    // valid because singular strategies are always evaluated before redundant strategies.
    double sum = 0.0;
    for (size_t i = 0; i < args->num_strategies; ++i) {
        if (children[i]->getEvalFn(ENERGY_FN) != NULL) {
            double value = jointDistribution->getMemoizedValue(memos[i], i);
            assert(value != DBL_MAX);
            sum += value;
        }
    }
    return sum;
}

double
memoized_total_data_cost(StrategyEvaluationContext *ctx,
                         void *strategy_arg, void *chooser_arg)
{
    GenericJointDistribution *jointDistribution = (GenericJointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    vector<MultiDimensionArray<double> *>& memos = *args->memos;
    Strategy *parent = args->parent_strategy;
    vector<Strategy *> children = parent->getChildStrategies();
    assert(args->num_strategies == children.size());

    // valid because singular strategies are always evaluated before redundant strategies.
    double sum = 0.0;
    for (size_t i = 0; i < args->num_strategies; ++i) {
        if (children[i]->getEvalFn(DATA_FN) != NULL) {
            double value = jointDistribution->getMemoizedValue(memos[i], i);
            assert(value != DBL_MAX);
            sum += value;
        }
    }
    return sum;
}

double
memo_saving_fn(StrategyEvaluationContext *ctx,
               void *strategy_arg, void *chooser_arg)
{
    GenericJointDistribution *jointDistribution = (GenericJointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    vector<MultiDimensionArray<double> *>& memos = *args->memos;
    MultiDimensionArray<double> *memo = memos[args->strategy_index];

    // only called with singular strategies.
#ifndef NDEBUG
    Strategy *strategy = args->parent_strategy;
#endif
    assert(!strategy->isRedundant());

    double value = args->real_fn(ctx, args->real_strategy_arg, chooser_arg);
    jointDistribution->saveMemoizedValue(memo, value);
    return value;
}

inline static typesafe_eval_fn_t
memoized_fn(typesafe_eval_fn_t fn)
{
    if (fn == redundant_strategy_minimum_time) {
        return memoized_min_time;
    } else if (fn == redundant_strategy_total_energy_cost) {
        return memoized_total_energy_cost;
    } else if (fn == redundant_strategy_total_data_cost) {
        return memoized_total_data_cost;
    } else {
        return memo_saving_fn;
    }
}

void 
GenericJointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearMemos();
    }
    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
GenericJointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    assert(iterator == NULL);
    
    struct memoized_strategy_args args;
    args.parent_strategy = strategy;
    args.num_strategies = strategy->getChildStrategies().size();
    args.real_fn = fn;
    args.real_strategy_arg = strategy_arg;
    args.memos = NULL;

    fn = memoized_fn(fn);
    if (fn == memo_saving_fn) {
        args.memos = &getMemoList(strategy, args.real_fn);
        args.strategy_index = getStrategyIndex(strategy);
    } else {
        args.memos = &getMemoList(strategy, fn);
    }
    
    double weightedSum = 0.0;
    iterator = new Iterator(this, strategy);
    while (!iterator->isDone()) {
        weightedSum += fn(this, &args, chooser_arg) * iterator->jointProbability();
        iterator->advance();
    }
    delete iterator;
    iterator = NULL;
    
    return weightedSum;
}

double 
GenericJointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    assert(iterator);

    // TODO: save per iteration?  it might not matter much.
    double estimate = estimator->getEstimate();

    double error = iterator->currentEstimatorError(estimator);
    return estimate - error;
}

void 
GenericJointDistribution::observationAdded(Estimator *estimator, double value)
{
    clearMemos();
    
    if (estimatorError.count(estimator) > 0) {
        double error = estimator->getEstimate() - value;
        estimatorError[estimator]->addValue(error);
    } else {
        estimatorError[estimator] = createErrorDistribution();
        
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        estimatorError[estimator]->addValue(0.0);
    }
}

vector<MultiDimensionArray<double> *>&
GenericJointDistribution::getMemoList(Strategy *strategy, typesafe_eval_fn_t fn)
{
    if (fn == memoized_min_time || fn == strategy->getEvalFn(TIME_FN)) {
        return time_memos;
    } else if (fn == memoized_total_energy_cost || fn == strategy->getEvalFn(ENERGY_FN)) {
        return energy_memos;
    } else if (fn == memoized_total_data_cost || fn == strategy->getEvalFn(DATA_FN)) {
        return data_memos;
    } else abort();
}

void
GenericJointDistribution::setEmptyMemos(Strategy *strategy, const vector<size_t>& dimensions)
{
    assert(time_memos.size() == energy_memos.size() &&
           energy_memos.size() == data_memos.size());
    assert(!strategy->isRedundant());

    size_t index = getStrategyIndex(strategy);
    if (time_memos[index] != NULL) {
        assert(energy_memos[index] != NULL &&
               data_memos[index] != NULL);

        // ignore; assume that expectedValue has been called twice with the same arguments
        return;
    }
    time_memos[index] = new MultiDimensionArray<double>(dimensions, DBL_MAX);
    energy_memos[index] = new MultiDimensionArray<double>(dimensions, DBL_MAX);
    data_memos[index] = new MultiDimensionArray<double>(dimensions, DBL_MAX);
}

void
GenericJointDistribution::clearMemos()
{
    assert(time_memos.size() == energy_memos.size() &&
           energy_memos.size() == data_memos.size());
    for (size_t i = 0; i < time_memos.size(); ++i) {
        delete time_memos[i];
        delete energy_memos[i];
        delete data_memos[i];
        time_memos[i] = NULL;
        energy_memos[i] = NULL;
        data_memos[i] = NULL;
    }
}

inline int
GenericJointDistribution::getStrategyIndex(Strategy *strategy)
{
    int strategy_index = -1;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        if (strategy == singular_strategies[i]) {
            strategy_index = i;
            break;
        }
    }
    assert(strategy_index != -1);
    return strategy_index;
}

inline double
GenericJointDistribution::getMemoizedValue(MultiDimensionArray<double> *memo, size_t strategy_index)
{
    assert(iterator);
    return memo->at(iterator->strategyPosition(strategy_index));
}

void
GenericJointDistribution::saveMemoizedValue(MultiDimensionArray<double> *memo,
                                     double value)
{
    assert(iterator);
    assert(iterator->numStrategies() == 1);
    size_t strategy_index = 0;
    memo->at(iterator->strategyPosition(strategy_index)) = value;
}

GenericJointDistribution::Iterator::Iterator(GenericJointDistribution *distribution_, Strategy *strategy)
{
    distribution = distribution_;
    
    // empty if the strategy is not redundant
    if (strategy->isRedundant()) {
        strategies = strategy->getChildStrategies();
        strategy_positions.resize(strategies.size());
    } else {
        strategies.push_back(strategy);
        strategy_positions.resize(1);
    }

    for (size_t i = 0; i < strategy_positions.size(); ++i) {
        // make sure these don't get reallocated after I save
        //  pointers to them.
        strategy_positions[i].reserve(distribution->estimatorError.size());
    }

    for (EstimatorErrorMap::iterator it = distribution->estimatorError.begin(); 
         it != distribution->estimatorError.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution *dist = it->second;
        if (strategy->usesEstimator(estimator)) {
            StatsDistribution::Iterator *iter = dist->getIterator();
            iterators.push_back(iter);
            errorIterators[estimator] = iter;
            iteratorIndices[estimator] = iterators.size() - 1;
            position.push_back(0);
            end_position.push_back(iter->totalCount());
            strategy_positions_for_estimator_position.push_back(vector<size_t*>());
            vector<size_t*>& saved_pos = strategy_positions_for_estimator_position.back();
            
            for (size_t i = 0; i < strategy_positions.size(); ++i) {
                // one index per estimator, 
                //   for each estimator that the strategy uses,
                //   in the same order as all estimators.
                if (strategies[i]->usesEstimator(estimator)) {
                    strategy_positions[i].push_back(0);

                    // This saves a reference to an index
                    // that will be later modified callback-style
                    // inside advance().
                    saved_pos.push_back(&strategy_positions[i].back());
                }
            }
        }
    }

    if (!strategy->isRedundant()) { 
        distribution->setEmptyMemos(strategy, end_position);
    }
    done = false;
    num_iterators = iterators.size();
    cached_probabilities.resize(num_iterators);
    setCachedProbabilities(0);
}

inline bool
GenericJointDistribution::Iterator::isDone()
{
    return done;
}

inline void
GenericJointDistribution::Iterator::setCachedProbabilities(size_t index)
{
    for (size_t i = index; i < num_iterators; ++i) {
        if (i == 0) {
            cached_probabilities[i] = iterators[i]->probability();
        } else {
            cached_probabilities[i] = cached_probabilities[i-1] * iterators[i]->probability();
        }
    }
}

inline double
GenericJointDistribution::Iterator::jointProbability()
{
    return cached_probabilities.back();
}

void
GenericJointDistribution::Iterator::advance()
{
    if (isDone()) {
        return;
    }

    int index = iterators.size() - 1;
    advancePosition(index);

    while (position[index] == end_position[index]) {
        resetPosition(index);
        --index;
        if (index >= 0) {
            advancePosition(index);
        } else {
            done = true;
            break;
        }
    }

    if (index >= 0) {
        setCachedProbabilities(index);
    }
}

void
GenericJointDistribution::Iterator::setStrategyPositions(size_t index, size_t value)
{
    assert(index < strategy_positions_for_estimator_position.size());
    const vector<size_t*>& subscribed_positions = strategy_positions_for_estimator_position[index];
    for (size_t i = 0; i < subscribed_positions.size(); ++i) {
        size_t *rval = subscribed_positions[i];
        *rval = value;
    }
}

inline void
GenericJointDistribution::Iterator::advancePosition(size_t index)
{
    setStrategyPositions(index, ++position[index]);
}

inline void
GenericJointDistribution::Iterator::resetPosition(size_t index)
{
    setStrategyPositions(index, (position[index] = 0));
}

GenericJointDistribution::Iterator::~Iterator()
{
    for (EstimatorErrorMap::iterator it = distribution->estimatorError.begin(); 
         it != distribution->estimatorError.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution *dist = it->second;
        StatsDistribution::Iterator *stats_iter = errorIterators[estimator];
        dist->finishIterator(stats_iter);
    }
}

inline double
GenericJointDistribution::Iterator::currentEstimatorError(Estimator *estimator)
{
    size_t index = iteratorIndices[estimator];
    return iterators[index]->at(position[index]);
}

inline const vector<size_t>&
GenericJointDistribution::Iterator::strategyPosition(Strategy *strategy)
{
    return strategyPosition(getStrategyIndex(strategy));
}

inline const vector<size_t>&
GenericJointDistribution::Iterator::strategyPosition(size_t index)
{
    return strategy_positions[index];
}

inline size_t
GenericJointDistribution::Iterator::getStrategyIndex(Strategy *strategy)
{
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (strategies[i] == strategy) {
            return i;
        }
    }
    abort();
}

inline size_t
GenericJointDistribution::Iterator::numStrategies()
{
    return strategies.size();
}

void
GenericJointDistribution::saveToFile(const char *filename)
{
    throw runtime_error("NOT IMPLEMENTED");
}

void
GenericJointDistribution::restoreFromFile(const char *filename)
{
    throw runtime_error("NOT IMPLEMENTED");
}