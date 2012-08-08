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

  private:
    IteratorStack *iterator_stack;// TODO: the deque is slow.  make it faster.
    
    EmpiricalErrorStrategyEvaluator *evaluator;

    ErrorIteratorMap iterators;
};

class MemoizingIteratorStack;

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

    IteratorStack *iterator_stack;

    class ProxyIterator : public JointErrorIterator {
      public:
        ProxyIterator(JointErrorIterator *errorIterator_,
                      MemoizingIteratorStack *timeMemo_,
                      MemoizingIteratorStack *energyMemo_,
                      MemoizingIteratorStack *dataMemo_)
            : errorIterator(errorIterator_),
              timeMemo(timeMemo_),
              energyMemo(energyMemo_),
              dataMemo(dataMemo_) {
            total_count = errorIterator->totalCount();
        }
        virtual void advance();
        virtual double probability() { throw UnimplementedError(); }
        virtual double getAdjustedEstimatorValue(Estimator *est) { throw UnimplementedError(); }
        virtual bool isDone();
        virtual void reset();
      private:
        JointErrorIterator *errorIterator;
        MemoizingIteratorStack *timeMemo;
        MemoizingIteratorStack *energyMemo;
        MemoizingIteratorStack *dataMemo;
    };
    vector<Iterator *> proxyIterators;

    vector<JointErrorIterator *> jointIterators;
    vector<MemoizingIteratorStack *> timeMemos;
    vector<MemoizingIteratorStack *> energyMemos;
    vector<MemoizingIteratorStack *> dataMemos;
    int memo_hits;
    int total_iterations;

    double combineStrategyValues(const vector<MemoizingIteratorStack *>& memos,
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
  protected:
    vector<Iterator *> iterators;

  private:
    bool done;

    void resetIteratorsAbove(int stack_top);
};

class MemoizingIteratorStack : public IteratorStack {
  public:
    MemoizingIteratorStack(const vector<Iterator *>& iterators_);
    ~MemoizingIteratorStack();
    bool currentValueIsMemoized();
    double memoizedCurrentValue();
    void saveCurrentValue(double value);

    virtual void advance();
    virtual void reset();
  private:
    double *getCurrentPosition();
    MultiDimensionArray<double> *array;
    vector<size_t> current_indices;
};

IteratorStack::IteratorStack(const vector<Iterator *>& iterators_)
    : iterators(iterators_), done(false)
{
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

    assert(iterators.size() > 0);
    int stack_top = iterators.size() - 1;

    // loop until we actually increment the joint iterator,
    //  or until we realize that it's done.
    Iterator *stats_iter = iterators[stack_top];
    stats_iter->advance();
    
    while (stats_iter->isDone()) {
        --stack_top;
        if (stack_top >= 0) {
            stats_iter = iterators[stack_top];

            // we haven't actually advanced the joint iterator, 
            //  so try advancing the next most significant
            //  estimator error iterator.
            stats_iter->advance();
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
    while (next_reset < iterators.size()) {
        iterators[next_reset]->reset();
        ++next_reset;
    }
}

void
IteratorStack::reset()
{
    for (size_t i = 0; i < iterators.size(); ++i) {
        iterators[i]->reset();
    }
    done = false;
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
    for (size_t i = 0; i < children.size(); ++i) {
        SingleStrategyJointErrorIterator *child_joint_iter = 
            new SingleStrategyJointErrorIterator(evaluator, children[i]);
        jointIterators.push_back(child_joint_iter);
        vector<Iterator *> child_iters =
            child_joint_iter->getIterators();
        total_count += child_joint_iter->totalCount();
        
        MemoizingIteratorStack *timeMemo = new MemoizingIteratorStack(child_iters);
        MemoizingIteratorStack *energyMemo = new MemoizingIteratorStack(child_iters);
        MemoizingIteratorStack *dataMemo = new MemoizingIteratorStack(child_iters);
        timeMemos.push_back(timeMemo);
        energyMemos.push_back(energyMemo);
        dataMemos.push_back(dataMemo);
        
        proxyIterators.push_back(new ProxyIterator(child_joint_iter, 
                                                   timeMemo, energyMemo, dataMemo));
    }
    iterator_stack = new IteratorStack(proxyIterators);
}

MultiStrategyJointErrorIterator::~MultiStrategyJointErrorIterator()
{
    delete iterator_stack;
    for (size_t i = 0; i < jointIterators.size(); ++i) {
        delete jointIterators[i];
        delete timeMemos[i];
        delete energyMemos[i];
        delete dataMemos[i];
        delete proxyIterators[i];
    }
    //fprintf(stderr, "%d memo hits of %d iterations\n", memo_hits, total_iterations);
}

double
MultiStrategyJointErrorIterator::combineStrategyValues(const vector<MemoizingIteratorStack *>& memos,
                                                       eval_fn_type_t eval_fn_type, 
                                                       double (*combiner)(double, double), double init_value,
                                                       void *chooser_arg)
{
    double cur_value = init_value;
    for (size_t i = 0; i < children.size(); ++i) {
        double new_value = 0.0;
        MemoizingIteratorStack *memo = memos[i];
        if (memo->currentValueIsMemoized()) {
            ++memo_hits;
            new_value = memo->memoizedCurrentValue();
        } else {
            Strategy *child = children[i];
            typesafe_eval_fn_t eval_fn = child->getEvalFn(eval_fn_type);
            new_value = eval_fn(jointIterators[i], child->strategy_arg, chooser_arg);
            memo->saveCurrentValue(new_value);
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
    for (size_t i = 0; i < jointIterators.size(); ++i) {
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

inline void
MultiStrategyJointErrorIterator::ProxyIterator::advance()
{
    if (!isDone()) {
        errorIterator->advance();
        timeMemo->advance();
        energyMemo->advance();
        dataMemo->advance();
        cur_position++;
    }
}

inline void
MultiStrategyJointErrorIterator::ProxyIterator::reset()
{
    errorIterator->reset();
    timeMemo->reset();
    energyMemo->reset();
    dataMemo->reset();
    cur_position = 0;
}

inline bool
MultiStrategyJointErrorIterator::ProxyIterator::isDone()
{
    return errorIterator->isDone();
}

MemoizingIteratorStack::MemoizingIteratorStack(const vector<Iterator *>& iterators_)
    : IteratorStack(iterators_)
{
    vector<size_t> dimensions;
    for (size_t i = 0; i < iterators.size(); ++i) {
        dimensions.push_back(iterators[i]->totalCount());
        current_indices.push_back(0);
    }
    array = new MultiDimensionArray<double>(dimensions, DBL_MAX);
}

MemoizingIteratorStack::~MemoizingIteratorStack()
{
    delete array;
}

inline void
MemoizingIteratorStack::advance()
{
    //IteratorStack::advance();
    // Don't advance the stack here.
    // This subclass really just observes a set of iterators.
    // TODO: rename/reorg it to reflect that.
    for (size_t i = 0; i < iterators.size(); ++i) {
        current_indices[i] = iterators[i]->position();
    }
}

inline void
MemoizingIteratorStack::reset()
{
    //IteratorStack::reset();
    for (size_t i = 0; i < iterators.size(); ++i) {
        current_indices[i] = 0;
    }
}

inline double *
MemoizingIteratorStack::getCurrentPosition()
{
    return &array->at(current_indices);
}

inline bool 
MemoizingIteratorStack::currentValueIsMemoized()
{
    return (memoizedCurrentValue() != DBL_MAX);
}

inline double 
MemoizingIteratorStack::memoizedCurrentValue()
{
    return *getCurrentPosition();
}

void 
MemoizingIteratorStack::saveCurrentValue(double value)
{
    *getCurrentPosition() = value;
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
