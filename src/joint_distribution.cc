#include "joint_distribution.h"

JointDistribution::JointDistribution(const small_set<Strategy *>& strategies)
{
    for (size_t i = 0; i < strategies.size(); ++i) {
        Strategy *strategy = strategies[i];
        if (!strategy->isRedundant()) {
            singular_strategies.push_back(strategy);
        }
    }
    time_memos.resize(singular_strategies.size(), NULL);
    energy_memos.resize(singular_strategies.size(), NULL);
    data_memos.resize(singular_strategies.size(), NULL);
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

static double
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
        minValue = min(minValue, jointDistribution->getMemoizedValue(strategy, strategy->time_fn));
    }
    return minValue;
}

static double
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
        sum += jointDistribution->getMemoizedValue(strategy, strategy->energy_fn);
    }
    return sum;
}

static double
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
        sum += jointDistribution->getMemoizedValue(strategy, strategy->data_fn);
    }
    return sum;
}

static double
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

void 
JointDistribution::setEvalArgs(void *strategy_arg_, void *chooser_arg_)
{
    strategy_arg = strategy_arg_;
    chooser_arg = chooser_arg_;
}

double 
JointDistribution::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn)
{
    assert(iterator == NULL);

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

MultiDimensionArray *
JointDistribution::getMemo(size_t strategy_index, typesafe_eval_fn_t fn)
{
    Strategy *strategy = singular_strategies[strategy_index];
    if (fn == memoized_min_time || fn == strategy->time_fn) {
        memo = time_memos[strategy_index];
    } else if (fn == memoized_total_energy_cost || fn == strategy->energy_fn) {
        memo = energy_memos[strategy_index];
    } else if (fn == memoized_total_data_cost || fn == strategy->data_fn) {
        memo = data_memos[strategy_index];
    } else assert(0);
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

    MultiDimensionArray *memo = getMemo(strategy_index, fn);
    return memo->at(iterator->strategyPosition(strategy));
}

void
JointDistribution::saveMemoizedValue(Strategy *strategy, typesafe_eval_fn_t fn,
                                     double value)
{
    assert(iterator);
    int strategy_index = getStrategyIndex(strategy);
    MultiDimensionArray *memo = getMemo(strategy_index, fn);
    memo->at(iterator->strategyPosition(strategy)) = value;
}

bool
JointDistribution::hasAllMemoizedValues(typesafe_eval_fn_t memoized_eval_fn)
{
    assert(iterator);
    for (size_t i = 0; i < singular_strategies.size(); ++i) {
        MultiDimensionArray *memo = getMemo(i, memoized_eval_fn);
        if (memo->at(iterator->strategyPosition(singular_strategies[i])) == DBL_MAX) {
            return false;
        }
    }
    return true;
}

JointDistribution::Iterator::Iterator(JointDistribution *distribution, Strategy *strategy)
{
    // empty if the strategy is not redundant
    if (strategy->isRedundant()) {
        strategies = strategy->getChildStrategies();
        strategy_positions.resize(strategies.size());
    } else {
        strategies.push_back(strategy);
        strategy_positions.resize(1);
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
            strategy_positions_for_estimator_position.push_back(vector<size_t*>());

            for (size_t i = 0; i < strategy_positions.size(); ++i) {
                // one index per estimator, 
                //   for each estimator that the strategy uses,
                //   in the same order as all estimators.
                if (strategies[i]->usesEstimator(estimator)) {
                    strategy_positions[i].push_back(0);

                    // This saves a reference to an index
                    // that will be later modified callback-style.  
                    // TODO: left off here. sanity-check this.
                    vector<size_t*>& saved_pos = strategy_positions_for_estimator_position.back();
                    saved_pos.push_back(&strategy_positions[i].back());
                }
            }
        }
    }
}

bool
JointDistribution::Iterator::isDone()
{
    return done;
}

double
JointDistribution::Iterator::probability()
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

    size_t index = iterators.size() - 1;
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
    const vector<size_t*>& strategy_positions_for_estimator_position[index]
    for (size_t i = 0; i < .size(); ++i) {
        size_t *rval = strategy_positions_for_estimator_position[i];
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

void
JointDistribution::Iterator::~Iterator()
{
    // TODO
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
