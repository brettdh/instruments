#include "joint_distribution.h"
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

#include <functional>
#include <vector>
using std::vector; using std::min;

JointDistribution::JointDistribution(const vector<Strategy *>& strategies)
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
    const vector<Strategy *>& singular_strategies;
    typesafe_eval_fn_t real_fn;
    void *real_strategy_arg;
    
    memoized_strategy_args(Strategy *parent,
                           const vector<Strategy *>& ss,
                           typesafe_eval_fn_t real_fn_,
                           void *real_strategy_arg_)
        : parent_strategy(parent),
          singular_strategies(ss),
          real_fn(real_fn_),
          real_strategy_arg(real_strategy_arg_) {}
};

double
memoized_min_time(StrategyEvaluationContext *ctx, 
                  void *strategy_arg, void *chooser_arg)
{
    JointDistribution *jointDistribution = (JointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    const vector<Strategy *>& singular_strategies = args->singular_strategies;

    // valid because singular strategies are always evaluated before redundant strategies.
    assert(jointDistribution->hasAllMemoizedValues(memoized_min_time));
    double minValue = DBL_MAX;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        Strategy *strategy = singular_strategies[i];
        minValue = min(minValue, jointDistribution->getMemoizedValue(strategy, strategy->getEvalFn(TIME_FN)));
    }
    return minValue;
}

double
memoized_total_energy_cost(StrategyEvaluationContext *ctx,
                           void *strategy_arg, void *chooser_arg)
{
    JointDistribution *jointDistribution = (JointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    const vector<Strategy *>& singular_strategies = args->singular_strategies;

    // valid because singular strategies are always evaluated before redundant strategies.
    assert(jointDistribution->hasAllMemoizedValues(memoized_total_energy_cost));
    double sum = 0.0;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        Strategy *strategy = singular_strategies[i];
        sum += jointDistribution->getMemoizedValue(strategy, strategy->getEvalFn(ENERGY_FN));
    }
    return sum;
}

double
memoized_total_data_cost(StrategyEvaluationContext *ctx,
                         void *strategy_arg, void *chooser_arg)
{
    JointDistribution *jointDistribution = (JointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    const vector<Strategy *>& singular_strategies = args->singular_strategies;

    // valid because singular strategies are always evaluated before redundant strategies.
    assert(jointDistribution->hasAllMemoizedValues(memoized_total_data_cost));
    double sum = 0.0;
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        Strategy *strategy = singular_strategies[i];
        sum += jointDistribution->getMemoizedValue(strategy, strategy->getEvalFn(DATA_FN));
    }
    return sum;
}

double
memo_saving_fn(StrategyEvaluationContext *ctx,
               void *strategy_arg, void *chooser_arg)
{
    JointDistribution *jointDistribution = (JointDistribution *) ctx;
    struct memoized_strategy_args *args =
        (struct memoized_strategy_args *) strategy_arg;
    Strategy *strategy = args->parent_strategy;

    // only called with singular strategies.
    assert(!strategy->isRedundant());

    double value = args->real_fn(ctx, args->real_strategy_arg, chooser_arg);
    jointDistribution->saveMemoizedValue(strategy, args->real_fn, value);
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
JointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    if (chooser_arg != chooser_arg_) {
        clearMemos();
    }
    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
JointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    assert(iterator == NULL);
    assert(strategy->getEvalFn(TIME_FN) != strategy->getEvalFn(ENERGY_FN) &&
           strategy->getEvalFn(TIME_FN) != strategy->getEvalFn(DATA_FN) &&
           strategy->getEvalFn(ENERGY_FN) != strategy->getEvalFn(DATA_FN));
    
    struct memoized_strategy_args args(strategy, strategy->getChildStrategies(), fn, strategy_arg);
    fn = memoized_fn(fn);
    
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
JointDistribution::getAdjustedEstimatorValue(Estimator *estimator)
{
    assert(iterator);

    // TODO: save per iteration?  it might not matter much.
    double estimate = estimator->getEstimate();

    double error = iterator->currentEstimatorError(estimator);
    return estimate - error;
}

void 
JointDistribution::observationAdded(Estimator *estimator, double value)
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

StatsDistribution *
JointDistribution::createErrorDistribution()
{
    // make this an actual factory method, with a flag set in 
    //  the JointDistribution constructor.
    return new StatsDistributionAllSamples;
    //return new StatsDistributionBinned;
}

MultiDimensionArray<double> *
JointDistribution::getMemo(size_t strategy_index, typesafe_eval_fn_t fn)
{
    Strategy *strategy = singular_strategies[strategy_index];
    if (fn == memoized_min_time || fn == strategy->getEvalFn(TIME_FN)) {
        return time_memos[strategy_index];
    } else if (fn == memoized_total_energy_cost || fn == strategy->getEvalFn(ENERGY_FN)) {
        return energy_memos[strategy_index];
    } else if (fn == memoized_total_data_cost || fn == strategy->getEvalFn(DATA_FN)) {
        return data_memos[strategy_index];
    } else abort();
}

void
JointDistribution::setEmptyMemos(Strategy *strategy, const vector<size_t>& dimensions)
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
JointDistribution::clearMemos()
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

int
JointDistribution::getStrategyIndex(Strategy *strategy)
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

double
JointDistribution::getMemoizedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    assert(iterator);
    int strategy_index = getStrategyIndex(strategy);

    MultiDimensionArray<double> *memo = getMemo(strategy_index, fn);
    return memo->at(iterator->strategyPosition(strategy));
}

void
JointDistribution::saveMemoizedValue(Strategy *strategy, typesafe_eval_fn_t fn,
                                     double value)
{
    assert(iterator);
    int strategy_index = getStrategyIndex(strategy);
    MultiDimensionArray<double> *memo = getMemo(strategy_index, fn);
    memo->at(iterator->strategyPosition(strategy)) = value;
}

bool
JointDistribution::hasAllMemoizedValues(typesafe_eval_fn_t memoized_eval_fn)
{
    assert(iterator);
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        MultiDimensionArray<double> *memo = getMemo(i, memoized_eval_fn);
        if (memo->at(iterator->strategyPosition(singular_strategies[i])) == DBL_MAX) {
            return false;
        }
    }
    return true;
}

JointDistribution::Iterator::Iterator(JointDistribution *distribution_, Strategy *strategy)
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
}

bool
JointDistribution::Iterator::isDone()
{
    return done;
}

double
JointDistribution::Iterator::jointProbability()
{
    double prob = 1.0;
    for (size_t i = 0; i < iterators.size(); ++i) {
        prob *= iterators[i]->probability();
    }
    return prob;
}

void
JointDistribution::Iterator::advance()
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
}

void
JointDistribution::Iterator::setStrategyPositions(size_t index, size_t value)
{
    assert(index < strategy_positions_for_estimator_position.size());
    const vector<size_t*>& subscribed_positions = strategy_positions_for_estimator_position[index];
    for (size_t i = 0; i < subscribed_positions.size(); ++i) {
        size_t *rval = subscribed_positions[i];
        *rval = value;
    }
}

inline void
JointDistribution::Iterator::advancePosition(size_t index)
{
    setStrategyPositions(index, ++position[index]);
}

inline void
JointDistribution::Iterator::resetPosition(size_t index)
{
    setStrategyPositions(index, (position[index] = 0));
}

JointDistribution::Iterator::~Iterator()
{
    for (EstimatorErrorMap::iterator it = distribution->estimatorError.begin(); 
         it != distribution->estimatorError.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution *dist = it->second;
        StatsDistribution::Iterator *stats_iter = errorIterators[estimator];
        dist->finishIterator(stats_iter);
    }
}

double
JointDistribution::Iterator::currentEstimatorError(Estimator *estimator)
{
    size_t index = iteratorIndices[estimator];
    return iterators[index]->at(position[index]);
}

const vector<size_t>&
JointDistribution::Iterator::strategyPosition(Strategy *strategy)
{
    for (size_t i = 0; i < strategies.size(); ++i) {
        if (strategies[i] == strategy) {
            return strategy_positions[i];
        }
    }
    abort();
}
