#include "empirical_error_strategy_evaluator.h"
#include "estimator.h"
#include "strategy.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#ifndef ANDROID
#include "stats_distribution_binned.h"
#endif
#include "multi_dimension_array.h"

#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>
using std::make_pair;

#include <vector>
using std::vector;

typedef small_map<Estimator*, StatsDistribution::Iterator *> ErrorIteratorMap;

class IteratorStack;

class UnimplementedError : public std::exception {
public:
    virtual const char *what() const throw() {
        static const char *msg = "Shouldn't be called!!";
        return msg;
    }
};

class JointErrorIterator : public Iterator, public StrategyEvaluationContext {
  protected:
    //int cur_position;
    //int total_count;
  public:
    JointErrorIterator() /*: cur_position(0), total_count(0)*/ {}
    virtual double value() { throw UnimplementedError(); }
    virtual void reset() { throw UnimplementedError(); /*cur_position = 0;*/ }
    virtual int position() { throw UnimplementedError(); /*assert(cur_position <= total_count); return cur_position;*/ }
    virtual int totalCount() { throw UnimplementedError(); /*return total_count;*/ }
    virtual double at(int pos) { throw UnimplementedError(); }
};

class SingleStrategyJointErrorIterator : public JointErrorIterator {
  public:
    SingleStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *s);
    virtual ~SingleStrategyJointErrorIterator();
    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    
    virtual double probability();
    virtual double probability(int pos) { throw UnimplementedError(); }
    virtual void advance();
    virtual bool isDone();
    virtual void reset();

    vector<Iterator *> getIterators();
    IteratorStack *getIteratorStack() { return iterator_stack; }
  private:
    IteratorStack *iterator_stack;
    
    EmpiricalErrorStrategyEvaluator *evaluator;

    ErrorIteratorMap iterators;
    small_map<Estimator *, int> iterator_indices;

    double cached_probability;
    bool cached_probability_is_valid;

    double computedProbability();
};

class MemoTable;

class MultiStrategyJointErrorIterator : public JointErrorIterator {
  public:
    MultiStrategyJointErrorIterator(EmpiricalErrorStrategyEvaluator *e, Strategy *s);
    virtual ~MultiStrategyJointErrorIterator();
    virtual double getAdjustedEstimatorValue(Estimator *estimator);
    
    virtual double probability();
    virtual double probability(int pos) { throw UnimplementedError(); }

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
    double probability();
    virtual void reset();
    virtual ~IteratorStack() {}

    double iteratorValue(int index);
    
    const vector<size_t>& getPosition();
    
    void setPosition();

  private:
    vector<Iterator *> iterators;
    size_t num_iterators;
    vector<size_t> position;
    vector<size_t> end_position;

    vector<double> cached_probabilities;
    void setCachedProbabilities(size_t first_index);

    bool done;

    size_t pos;
    size_t total_size;
};

class MemoTable {
  public:
    MemoTable(const vector<Iterator *>& iterators_);
    ~MemoTable();

    struct Entry {
        bool valid;
        double value;
        Entry() : valid(false), value(0.0) {}
        Entry(bool valid_, double value_) : valid(valid_), value(value_) {}
    };
    Entry *getPosition(const vector<size_t>& indices);
  private:
    MultiDimensionArray<Entry> *array;
};

IteratorStack::IteratorStack(const vector<Iterator *>& iterators_)
    : iterators(iterators_), done(false), pos(0), total_size(1)
{
    num_iterators = iterators.size();
    position.resize(iterators.size(), 0);
    end_position.resize(iterators.size(), 0);
    
    if (iterators.empty()) {
        // unlikely corner case.
        done = true;
    }

    for (size_t i = 0; i < iterators.size(); ++i) {
        int iter_size = iterators[i]->totalCount();
        total_size *= iter_size;
        end_position[i] = iter_size;
    }

    cached_probabilities.resize(iterators.size(), 0.0);
    setCachedProbabilities(0);
}

void
IteratorStack::setCachedProbabilities(size_t first_index)
{
    for (size_t i = first_index; i < cached_probabilities.size(); ++i) {
        if (i > 0) {
            cached_probabilities[i] = cached_probabilities[i-1] * iterators[i]->probability(position[i]);
        } else {
            cached_probabilities[i] = iterators[i]->probability(position[i]);
        }
    }
}

double
IteratorStack::probability()
{
    return *cached_probabilities.rbegin();
    /*
    double prob = 1.0;
    for (size_t i = 0; i < iterators.size(); ++i) {
        prob *= iterators[i]->probability(position[i]);
    }
    return prob;
    */
}

inline bool
IteratorStack::isDone()
{
    return done;
}

inline double 
IteratorStack::iteratorValue(int index)
{
    /*
    int cur_pos = pos;
    for (int i = end_position.size() - 1; i >= index; --i) {
        if (i == index) {
            cur_pos = cur_pos % end_position[i];
            //assert(cur_pos < end_position[i]);
            break;
        } else {
            cur_pos /= end_position[i];
        }
    }
    */
    return iterators[index]->at(position[index]);
    //return iterators[index]->at(cur_pos);
}

void
IteratorStack::setPosition()
{
    int cur_pos = pos;
    for (int i = end_position.size() - 1; i >= 0; --i) {
        position[i] = cur_pos % end_position[i];
        cur_pos /= end_position[i];
    }
}

void
IteratorStack::advance()
{
    if (isDone()) {
        return;
    }

    /*
    if (++pos == total_size) {
        done = true;
    }
    //setPosition();
    return;
    */

    assert(num_iterators > 0);
    //vector<Iterator*>::reverse_iterator iter_it = iterators.rbegin();
    int iter_pos = num_iterators - 1;
    //vector<size_t>::reverse_iterator position_it = position.rbegin();

    // loop until we actually increment the joint iterator,
    //  or until we realize that it's done.
    //Iterator *cur_iter = iterators[iter_pos];//*iter_it;
    //cur_iter->advance();
    ++position[iter_pos];

    while (position[iter_pos] == end_position[iter_pos]) {
        //cur_iter->reset();
        position[iter_pos] = 0;
        //*position_it = 0;
        
        if (--iter_pos >= 0) {
            //cur_iter = iterators[iter_pos];
            //++position_it;

            // we haven't actually advanced the joint iterator, 
            //  so try advancing the next most significant
            //  estimator error iterator.
            //cur_iter->advance();
            ++position[iter_pos];
        } else {
            // all iterators are done; the joint iterator is done too.
            done = true;
            break;
        }
    }

    if (!isDone()) {
        assert(iter_pos >= 0);
        setCachedProbabilities(iter_pos);
    }
}

void
IteratorStack::reset()
{
    for (size_t i = 0; i < num_iterators; ++i) {
        //iterators[i]->reset();
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
        //total_count += child_joint_iter->totalCount();
        
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
    //fprintf(stderr, "%d memo hits of %d iterations (%.2f%%)\n", 
    //        memo_hits, total_iterations, ((double)memo_hits) / total_iterations * 100);
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
        MemoTable::Entry *memoEntry = memo->getPosition(position);
        if (memoEntry->valid) {
            //++memo_hits;
            new_value = memoEntry->value;
        } else {
            Strategy *child = children[i];
            typesafe_eval_fn_t eval_fn = child->getEvalFn(eval_fn_type);
            new_value = eval_fn(jointIterators[i], child->strategy_arg, chooser_arg);
            memoEntry->valid = true;
            memoEntry->value = new_value;
        }
        cur_value = combiner(cur_value, new_value);
        //++total_iterations;
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

inline double
MultiStrategyJointErrorIterator::minimumTime(void *chooser_arg)
{
    return combineStrategyValues(timeMemos, TIME_FN, my_fmin, DBL_MAX, chooser_arg);
}

inline double
MultiStrategyJointErrorIterator::totalEnergy(void *chooser_arg)
{
    return combineStrategyValues(energyMemos, ENERGY_FN, my_fsum, 0.0, chooser_arg);
}

inline double
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

inline double 
MultiStrategyJointErrorIterator::probability()
{
    return iterator_stack->probability();
    
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
        //++cur_position;
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
    //cur_position = 0;
}


MemoTable::MemoTable(const vector<Iterator *>& iterators)
{
    vector<size_t> dimensions;
    for (size_t i = 0; i < iterators.size(); ++i) {
        dimensions.push_back(iterators[i]->totalCount());
    }
    array = new MultiDimensionArray<Entry>(dimensions, Entry(false, 0.0));
}

MemoTable::~MemoTable()
{
    delete array;
}

inline MemoTable::Entry *
MemoTable::getPosition(const vector<size_t>& position)
{
    return &array->at(position);
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
    : evaluator(e), cached_probability(0.0), cached_probability_is_valid(false)
{
    vector<Iterator *> tmp_iters;
    for (JointErrorMap::const_iterator it = evaluator->jointError.begin();
         it != evaluator->jointError.end(); ++it) {
        Estimator *estimator = it->first;
        if (strategy->usesEstimator(estimator)) {
            StatsDistribution::Iterator *stats_iter = it->second->getIterator();
            iterators[estimator] = stats_iter;
            iterator_indices[estimator] = tmp_iters.size();
            //total_count += stats_iter->totalCount();
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
    size_t index = iterator_indices[estimator];
    //StatsDistribution::Iterator *it = 
    double error = iterator_stack->iteratorValue(index); //it->value();
    return estimator->getEstimate() - error;
}

inline double
SingleStrategyJointErrorIterator::probability()
{
    //return cached_probability_is_valid ? cached_probability : computedProbability();
    if (cached_probability_is_valid) {
        return cached_probability;
    }
    return computedProbability();
}

double
SingleStrategyJointErrorIterator::computedProbability()
{
    return iterator_stack->probability();

    /*
    assert(iterators.size() > 0);

    ErrorIteratorMap::iterator it = iterators.begin();
    StatsDistribution::Iterator *stats_iter = it->second;
    double probability = stats_iter->probability();

    ++it;
    for (; it != iterators.end(); ++it) {
        stats_iter = it->second;
        probability *= stats_iter->probability();
    }
    cached_probability = probability;
    cached_probability_is_valid = true;
    
    return probability;
    */
}

inline void
SingleStrategyJointErrorIterator::advance()
{
    cached_probability_is_valid = false;
    if (!isDone()) {
        iterator_stack->advance();
        //++cur_position;
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
    //cur_position = 0;
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
