#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "strategy.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "stats_distribution_binned.h"
#include "multi_dimension_array.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
using std::make_pair;

#include <vector>
using std::vector;

typedef small_map<Estimator*, StatsDistribution::Iterator *> ErrorIteratorMap;

class IteratorStack;

class UnimplementedError : public std::exception {
public:
    virtual const char *what() {
        static const char *msg = "Shouldn't be called!!";
        return msg;
    }
};

class JointErrorIterator : public Iterator, public StrategyEvaluationContext {
  protected:
    int cur_position;
    int total_count;
  public:
    JointErrorIterator() : cur_position(0), total_count(0) {}
    virtual double value() { throw UnimplementedError(); }
    virtual void reset() { cur_position = 0; }
    virtual int position() { assert(cur_position <= total_count); return cur_position; }
    virtual int totalCount() { return total_count; }
};

class SingleStrategyJointErrorIterator : public JointErrorIterator {
  public:
    SingleStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *s);
    virtual ~SingleStrategyJointErrorIterator();
    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    
    virtual double probability();
    virtual void advance();
    virtual bool isDone();
    virtual void reset();

    vector<Iterator *> getIterators();
    IteratorStack *getIteratorStack() { return iterator_stack; }
  private:
    IteratorStack *iterator_stack;
    
    EmpiricalErrorStrategyEvaluator *evaluator;

    ErrorIteratorMap iterators;
};

class MemoTable;

class MultiStrategyJointErrorIterator : public JointErrorIterator {
  public:
    MultiStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *s);
    virtual ~MultiStrategyJointErrorIterator();
    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    
    virtual double probability();
    virtual void advance();
    virtual bool isDone();
    virtual void reset();

    double minimumTime(void *chooser_arg);
    double totalEnergy(void *chooser_arg);
    double totalData(void *chooser_arg);

  private:
    EmpiricalErrorStrategyEvaluator *evaluator;
    Strategy *parent;
    std::vector<Strategy *> children;
    size_t num_children;

    IteratorStack *iterator_stack;
    vector<JointErrorIterator *> jointIterators;
    vector<MemoTable *> timeMemos;
    vector<MemoTable *> energyMemos;
    vector<MemoTable *> dataMemos;
    size_t num_iterators;

    int memo_hits;
    int total_iterations;

    double combineStrategyValues(const vector<MemoTable *>& memos,
                                 eval_fn_type_t eval_fn_type, 
                                 double (*combiner)(double, double), double init_value,
                                 void *chooser_arg);
};

class IteratorStack {
  public:
    IteratorStack(const vector<Iterator *>& iterators_);
    virtual void advance();
    bool isDone();
    virtual void reset();
    
    const vector<size_t>& getPosition();
  protected:
    vector<Iterator *> iterators;
    size_t num_iterators;
    vector<size_t> position;

  private:
    bool done;

    void resetIteratorsAbove(int stack_top);
};

class MemoTable {
  public:
    MemoTable(const vector<Iterator *>& iterators_);
    ~MemoTable();
    bool valueIsMemoized(const vector<size_t>& indices);
    double memoizedValue(const vector<size_t>& indices);
    void saveValue(const vector<size_t>& indices, double value);

  private:
    double *getPosition(const vector<size_t>& indices);
    MultiDimensionArray<double> *array;
};

IteratorStack::IteratorStack(const vector<Iterator *>& iterators_)
    : iterators(iterators_), done(false)
{
    num_iterators = iterators.size();
    position.resize(iterators.size(), 0);
    
    if (iterators.empty()) {
        // unlikely corner case.
        done = true;
    }
}

inline bool
IteratorStack::isDone()
{
    return done;
}

void
IteratorStack::advance()
{
    if (isDone()) {
        return;
    }

    assert(num_iterators > 0);
    int stack_top = num_iterators - 1;

    // loop until we actually increment the joint iterator,
    //  or until we realize that it's done.
    Iterator *stats_iter = iterators[stack_top];
    stats_iter->advance();
    ++position[stack_top];
    
    while (stats_iter->isDone()) {
        --stack_top;
        if (stack_top >= 0) {
            stats_iter = iterators[stack_top];

            // we haven't actually advanced the joint iterator, 
            //  so try advancing the next most significant
            //  estimator error iterator.
            stats_iter->advance();
            ++position[stack_top];
        } else {
            // all iterators are done; the joint iterator is done too.
            done = true;
            break;
        }
    }

    if (stack_top >= 0) {
        // if the joint iteration isn't done yet, 
        //  reset the 'less significant iterators'
        //  than the one that was actually advanced.
        resetIteratorsAbove(stack_top);
    }
}

void
IteratorStack::resetIteratorsAbove(int stack_top)
{
    assert(stack_top >= 0);
    size_t next_reset = stack_top + 1;
    while (next_reset < num_iterators) {
        iterators[next_reset]->reset();
        position[next_reset] = 0;
        ++next_reset;
    }
}

void
IteratorStack::reset()
{
    for (size_t i = 0; i < num_iterators; ++i) {
        iterators[i]->reset();
        position[i] = 0;
    }
    done = false;
}

inline const vector<size_t>& 
IteratorStack::getPosition()
{
    return position;
}


EmpiricalErrorStrategyEvaluator::EmpiricalErrorStrategyEvaluator()
    : jointErrorIterator(NULL)
{
}

double 
EmpiricalErrorStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    assert(jointErrorIterator != NULL);
    return jointErrorIterator->getAdjustedEstimatorValue(estimator);
}

void 
EmpiricalErrorStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (jointError.count(estimator) > 0) {
        double error = estimator->getEstimate() - value;
        jointError[estimator]->addValue(error);
    } else {
        // TODO: move this to a factory method (with the other methods)
        jointError[estimator] = new StatsDistributionAllSamples;
        //jointError[estimator] = new StatsDistributionBinned;
        
        // don't add a real error value to the distribution.
        // there's no error until we have at least two observations.
        jointError[estimator]->addValue(0.0);
    }
}


static double
multi_strategy_time_fn(StrategyEvaluationContext *ctx, void *strategy_arg, void *chooser_arg)
{
    MultiStrategyJointErrorIterator *it = 
        (MultiStrategyJointErrorIterator *) strategy_arg;
    return it->minimumTime(chooser_arg);
}

static double
multi_strategy_energy_fn(StrategyEvaluationContext *ctx, void *strategy_arg, void *chooser_arg)
{
    MultiStrategyJointErrorIterator *it = 
        (MultiStrategyJointErrorIterator *) strategy_arg;
    return it->totalEnergy(chooser_arg);
}

static double
multi_strategy_data_fn(StrategyEvaluationContext *ctx, void *strategy_arg, void *chooser_arg)
{
    MultiStrategyJointErrorIterator *it = 
        (MultiStrategyJointErrorIterator *) strategy_arg;
    return it->totalData(chooser_arg);
}

typesafe_eval_fn_t
memoized_multi_strategy_fn(typesafe_eval_fn_t fn)
{
    if (fn == redundant_strategy_minimum_time) {
        return multi_strategy_time_fn;
    } else if (fn == redundant_strategy_total_energy_cost) {
        return multi_strategy_energy_fn;
    } else if (fn == redundant_strategy_total_data_cost) {
        return multi_strategy_data_fn;
    } else abort();
}

MultiStrategyJointErrorIterator::MultiStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e,
                                                                 Strategy *parent_)
    : evaluator(e), parent(parent_), memo_hits(0), total_iterations(0)
{
    children = parent->getChildStrategies();
    num_children = children.size();
    for (size_t i = 0; i < num_children; ++i) {
        SingleStrategyJointErrorIterator *child_joint_iter = 
            new SingleStrategyJointErrorIterator(evaluator, children[i]);
        jointIterators.push_back(child_joint_iter);
        vector<Iterator *> child_iters =
            child_joint_iter->getIterators();
        total_count += child_joint_iter->totalCount();
        
        MemoTable *timeMemo = new MemoTable(child_iters);
        MemoTable *energyMemo = new MemoTable(child_iters);
        MemoTable *dataMemo = new MemoTable(child_iters);
        timeMemos.push_back(timeMemo);
        energyMemos.push_back(energyMemo);
        dataMemos.push_back(dataMemo);
        
    }
    num_iterators = jointIterators.size();
    iterator_stack = new IteratorStack(vector<Iterator*>(jointIterators.begin(),
                                                         jointIterators.end()));
}

MultiStrategyJointErrorIterator::~MultiStrategyJointErrorIterator()
{
    delete iterator_stack;
    for (size_t i = 0; i < num_iterators; ++i) {
        delete jointIterators[i];
        delete timeMemos[i];
        delete energyMemos[i];
        delete dataMemos[i];
    }
    //fprintf(stderr, "%d memo hits of %d iterations\n", memo_hits, total_iterations);
}

double
MultiStrategyJointErrorIterator::combineStrategyValues(const vector<MemoTable *>& memos,
                                                       eval_fn_type_t eval_fn_type, 
                                                       double (*combiner)(double, double), double init_value,
                                                       void *chooser_arg)
{
    double cur_value = init_value;
    for (size_t i = 0; i < num_children; ++i) {
        double new_value = 0.0;
        MemoTable *memo = memos[i];
        SingleStrategyJointErrorIterator *it = 
            (SingleStrategyJointErrorIterator *) jointIterators[i];
        IteratorStack *childStack = it->getIteratorStack();
        const vector<size_t>& position = childStack->getPosition();
        if (memo->valueIsMemoized(position)) {
            ++memo_hits;
            new_value = memo->memoizedValue(position);
        } else {
            Strategy *child = children[i];
            typesafe_eval_fn_t eval_fn = child->getEvalFn(eval_fn_type);
            new_value = eval_fn(jointIterators[i], child->strategy_arg, chooser_arg);
            memo->saveValue(position, new_value);
        }
        cur_value = combiner(cur_value, new_value);
        ++total_iterations;
    }
    return cur_value;
}

static inline double
my_fmin(double a, double b)
{
    return (a < b) ? a : b;
}

static inline double
my_fsum(double a, double b)
{
    return a + b;
}

double
MultiStrategyJointErrorIterator::minimumTime(void *chooser_arg)
{
    return combineStrategyValues(timeMemos, TIME_FN, my_fmin, DBL_MAX, chooser_arg);
}

double
MultiStrategyJointErrorIterator::totalEnergy(void *chooser_arg)
{
    return combineStrategyValues(energyMemos, ENERGY_FN, my_fsum, 0.0, chooser_arg);
}

double
MultiStrategyJointErrorIterator::totalData(void *chooser_arg)
{
    return combineStrategyValues(dataMemos, DATA_FN, my_fsum, 0.0, chooser_arg);
}

double 
MultiStrategyJointErrorIterator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // should never be called.
    // in combineStrategyValues, when I call the eval functions, I pass
    //  the individual strategy's joint iterator as the context,
    //  so this will never be called.
    abort();
}

double 
MultiStrategyJointErrorIterator::probability()
{
    // TODO: memoize this further?
    double prob = 1.0;
    for (size_t i = 0; i < num_iterators; ++i) {
        prob *= jointIterators[i]->probability();
    }
    return prob;
}

inline void 
MultiStrategyJointErrorIterator::advance()
{
    if (!isDone()) {
        iterator_stack->advance();
        ++cur_position;
    }
}

inline bool 
MultiStrategyJointErrorIterator::isDone()
{
    return iterator_stack->isDone();
}

inline void
MultiStrategyJointErrorIterator::reset()
{
    iterator_stack->reset();
    cur_position = 0;
}


MemoTable::MemoTable(const vector<Iterator *>& iterators)
{
    vector<size_t> dimensions;
    for (size_t i = 0; i < iterators.size(); ++i) {
        dimensions.push_back(iterators[i]->totalCount());
    }
    array = new MultiDimensionArray<double>(dimensions, DBL_MAX);
}

MemoTable::~MemoTable()
{
    delete array;
}

inline double *
MemoTable::getPosition(const vector<size_t>& position)
{
    return &array->at(position);
}

inline bool 
MemoTable::valueIsMemoized(const vector<size_t>& position)
{
    return (memoizedValue(position) != DBL_MAX);
}

inline double 
MemoTable::memoizedValue(const vector<size_t>& position)
{
    return *getPosition(position);
}

void 
MemoTable::saveValue(const vector<size_t>& position, double value)
{
    *getPosition(position) = value;
}


double
EmpiricalErrorStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                               void *strategy_arg, void *chooser_arg)
{
    double weightedSum = 0.0;
    if (jointErrorIterator != NULL) {
        // TODO: print message and abort; chooseStrategy has been called concurrently.
        assert(0);
    }

    if (false) {//strategy->isRedundant() && strategy->childrenAreDisjoint()) {
        MultiStrategyJointErrorIterator *it =
            new MultiStrategyJointErrorIterator(this, strategy);
        jointErrorIterator = it;
        
        assert(fn == redundant_strategy_minimum_time ||
               fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost);
        fn = memoized_multi_strategy_fn(fn);
        strategy_arg = it;
    } else {
        jointErrorIterator = new SingleStrategyJointErrorIterator(this, strategy);
    }
    while (!jointErrorIterator->isDone()) {
        double value = fn(jointErrorIterator, strategy_arg, chooser_arg);
        double probability = jointErrorIterator->probability();
        weightedSum += value * probability;
        
        jointErrorIterator->advance();
    }
    delete jointErrorIterator;
    jointErrorIterator = NULL;
    
    return weightedSum;
}

SingleStrategyJointErrorIterator::SingleStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *strategy)
    : evaluator(e)
{
    vector<Iterator *> tmp_iters;
    for (JointErrorMap::const_iterator it = evaluator->jointError.begin();
         it != evaluator->jointError.end(); ++it) {
        Estimator *estimator = it->first;
        if (strategy->usesEstimator(estimator)) {
            StatsDistribution::Iterator *stats_iter = it->second->getIterator();
            iterators[estimator] = stats_iter;
            total_count += stats_iter->totalCount();
            tmp_iters.push_back(stats_iter);
        }
    }
    iterator_stack = new IteratorStack(tmp_iters);
}

SingleStrategyJointErrorIterator::~SingleStrategyJointErrorIterator()
{
    delete iterator_stack;

    for (ErrorIteratorMap::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        Estimator *estimator = it->first;
        StatsDistribution::Iterator *stats_iter = it->second;
        StatsDistribution *distribution = evaluator->jointError[estimator];
        distribution->finishIterator(stats_iter);
    }
}

double
SingleStrategyJointErrorIterator::getAdjustedEstimatorValue(Estimator *estimator)
{
    StatsDistribution::Iterator *it = iterators[estimator];
    double error = it->value();
    return estimator->getEstimate() - error;
}

double
SingleStrategyJointErrorIterator::probability()
{
    double probability = 1.0;
    for (ErrorIteratorMap::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        StatsDistribution::Iterator *stats_iter = it->second;
        probability *= stats_iter->probability();
    }
    return probability;
}

inline void
SingleStrategyJointErrorIterator::advance()
{
    if (!isDone()) {
        iterator_stack->advance();
        ++cur_position;
    }
}

inline bool
SingleStrategyJointErrorIterator::isDone()
{
    return iterator_stack->isDone();
}

inline void
SingleStrategyJointErrorIterator::reset()
{
    iterator_stack->reset();
    cur_position = 0;
}

vector<Iterator *> 
SingleStrategyJointErrorIterator::getIterators()
{
    vector<Iterator *> stats_iters;
    for (ErrorIteratorMap::iterator it = iterators.begin();
         it != iterators.end(); ++it) {
        stats_iters.push_back(it->second);
    }
    return stats_iters;
}
