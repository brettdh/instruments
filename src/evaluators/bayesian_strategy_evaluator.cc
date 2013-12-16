#include "bayesian_strategy_evaluator.h"
#include "estimator.h"
#include "stats_distribution_binned.h"
#include "debug.h"
#include "timeops.h"
#include "stopwatch.h"
#include "error_weight_params.h"
namespace inst = instruments;
using inst::INFO; using inst::DEBUG;
using inst::MAX_SAMPLES;

#include <vector>
#include <map>
#include <functional>
#include <algorithm>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <memory>
using std::vector; using std::map; using std::make_pair;
using std::runtime_error; using std::ostringstream;
using std::istringstream; using std::ostream;
using std::string; using std::setprecision; using std::setw;
using std::ifstream; using std::ofstream; using std::endl;
using std::pair; using std::any_of;
using std::shared_ptr;

#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <float.h>

static inline double my_fabs(double x)
{
    // trick GDB into not being stupid.
    return ((double (*)(double)) fabs)(x);
}

typedef double (*combiner_fn_t)(double, double);

template <typename T>
class VectorLess {
    constexpr static double THRESHOLD = 0.0001;
    
  public:
    // return true iff first < second
    bool operator()(const vector<T>& first, const vector<T>& second) const {
        ASSERT(first.size() == second.size());
        for (size_t i = 0; i < first.size(); ++i) {
            T diff = first[i] - second[i];
            if (fabs(diff) > THRESHOLD) {
                return diff < 0.0;
            }
        }
        return false;
    }
};

template <typename T>
class VectorsEqual {
    VectorLess<T> less;
  public:
    bool operator()(const vector<T>& first, const vector<T>& second) const {
        return !(less(first, second) || less(second, first));
    }
};

class DistributionKey {
  public:
    DistributionKey(BayesianStrategyEvaluator *evaluator);
    ~DistributionKey();
    void addEstimatorValue(Estimator *estimator, double value);
    void forEachEstimator(std::function<bool(Estimator *, double)> fn) const;

    bool operator<(const DistributionKey& other) const;
    bool operator==(const DistributionKey& other) const;
    ostream& print(ostream& os) const;
    size_t getPrintSize() const;

  private:
    static size_t VALUE_PRINT_WIDTH;
    typedef map<Estimator *, size_t> EstimatorIndicesMap;
    typedef shared_ptr<EstimatorIndicesMap> EstimatorIndicesMapPtr;
    typedef map<BayesianStrategyEvaluator *,  EstimatorIndicesMapPtr> EstimatorIndicesByEvaluatorMap; 
    static EstimatorIndicesByEvaluatorMap estimator_indices_maps;

    BayesianStrategyEvaluator *evaluator;
    shared_ptr<EstimatorIndicesMap> estimator_indices;
    
    vector<int> key; // list of indices into binned distribution
    VectorLess<int> less;
    VectorsEqual<int> equal;
};

DistributionKey::EstimatorIndicesByEvaluatorMap DistributionKey::estimator_indices_maps;

DistributionKey::DistributionKey(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_)
{
    if (estimator_indices_maps.count(evaluator) == 0) {
        estimator_indices_maps[evaluator].reset(new EstimatorIndicesMap);
    }
    estimator_indices = estimator_indices_maps[evaluator];
}

DistributionKey::~DistributionKey()
{
    if (estimator_indices.use_count() == 2) {
        // I'm the last DistributionKey from this evaluator to go away,
        //  so only me and the global map hold references to this map
        estimator_indices_maps.erase(evaluator);
    }
    // managed map will get cleaned up by my shared_ptr's destructor
}

void 
DistributionKey::addEstimatorValue(Estimator *estimator, double value)
{
    ASSERT(estimator_indices != nullptr);
    if (estimator_indices->count(estimator) == 0) {
        estimator_indices->insert(make_pair(estimator, estimator_indices->size()));
    }
    size_t index = estimator_indices->at(estimator);
    if (key.size() <= index) {
        key.resize(index + 1);
    }

    ASSERT(evaluator->estimatorSamples.count(estimator) > 0);
    size_t bin_index = evaluator->estimatorSamples[estimator]->getIndex(value);
    key[index] = bin_index;
}

void
DistributionKey::forEachEstimator(std::function<bool(Estimator *, double)> fn) const
{
    ASSERT(estimator_indices != nullptr);
    ASSERT(estimator_indices->size() == key.size());
    for (auto& p : *estimator_indices) {
        Estimator *estimator = p.first;
        size_t index = p.second;
        ASSERT(index < key.size());
        
        // unnecessary most of the time, except when the tail values can change.
        double value = evaluator->estimatorSamples[estimator]->getValueAtIndex(key[index]);
        
        if (fn(estimator, value) == false) {
            break;
        }
    }
}

bool
DistributionKey::operator<(const DistributionKey& other) const
{
    return less(key, other.key);
}

bool 
DistributionKey::operator==(const DistributionKey& other) const
{
    return equal(key, other.key);
}

ostream&
DistributionKey::print(ostream& os) const
{
    os << "[ ";
    forEachEstimator([&](Estimator *estimator, double value) {
            os << setw(VALUE_PRINT_WIDTH) << value << " ";
            return true;
        });
    os << "]";
    return os;
}

ostream&
operator<<(ostream& os, const DistributionKey& key)
{
    return key.print(os);
}

size_t DistributionKey::VALUE_PRINT_WIDTH = 10;

size_t 
DistributionKey::getPrintSize() const
{
    const size_t extra_chars = 3; // "[ ... ]"
    return extra_chars + (VALUE_PRINT_WIDTH + 1) * estimator_indices->size();
}


class BayesianStrategyEvaluator::SimpleEvaluator : public StrategyEvaluator {
  public:
    SimpleEvaluator(const string& name,
                    const instruments_strategy_t *new_strategies,
                    size_t num_strategies);
    ~SimpleEvaluator();
    
    Strategy *chooseStrategy(void *chooser_arg);
    virtual double expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                 void *strategy_arg, void *chooser_arg,
                                 ComparisonType comparison_type);
    virtual double getAdjustedEstimatorValue(Estimator *estimator);

    // nothing to save/restore.
    virtual void saveToFile(const char *filename) {}
    virtual void restoreFromFileImpl(const char *filename) {}

    void setEstimatorValue(Estimator *estimator, double value);
    void setEstimatorValues(const vector<pair<Estimator *, double> >& new_estimator_values);
    vector<pair<Estimator *, double> > getEstimatorValues();
    
    void clear();
  private:
    map<Estimator *, double> estimator_values;
};

class BayesianStrategyEvaluator::Likelihood {
  public:
    Likelihood(BayesianStrategyEvaluator *evaluator_);
    void addObservation(Estimator *estimator, double observation);
    void addDecision(const vector<pair<Estimator *,double> >& estimator_values);
    double getWeightedSum(SimpleEvaluator *tmp_simple_valuator, 
                          Strategy *strategy, typesafe_eval_fn_t fn,
                          void *strategy_arg, void *chooser_arg);
    double getCurrentEstimatorSample(Estimator *estimator);
    void clear();
  private:
    BayesianStrategyEvaluator *evaluator;
    map<Estimator *, double> last_observation;

    // for use during iterated weighted sum.
    map<Estimator *, double> currentEstimatorSamples;
    
    // Each value_type is a map of likelihood-value-tuples to 
    //   their likeihood decision histograms.
    // I keep these separately for each strategy for easy lookup
    //   and iteration over only those estimators that matter
    //   for the current strategy.
    typedef map<DistributionKey, DecisionsHistogram *> LikelihoodMap;
    LikelihoodMap likelihood_distribution;

    DistributionKey getCurrentEstimatorKey(const vector<pair<Estimator *, double> >& estimator_values);
    DistributionKey makeEstimatorKey(const vector<pair<Estimator *, double> >& estimator_values);
    void setEstimatorSamples(DistributionKey& key);
    double jointPriorProbability(DistributionKey& key);
};

class BayesianStrategyEvaluator::DecisionsHistogram {
  public:
    DecisionsHistogram(BayesianStrategyEvaluator *evaluator_);
    void addDecision(const vector<pair<Estimator *,double> >& estimator_values);
    double getWinnerProbability(SimpleEvaluator *tmp_simple_evaluator,
                                void *chooser_strategy, bool ensure_nonzero);
    void clear();
  private:
    BayesianStrategyEvaluator *evaluator;
    vector<vector<pair<Estimator *, double> > > decisions;
    
    void *last_chooser_arg;
    map<Strategy *, double> last_winner_probability;
};

BayesianStrategyEvaluator::BayesianStrategyEvaluator(bool weighted_)
    : simple_evaluator(NULL), weighted(weighted_)
{
    likelihood = new Likelihood(this);
    normalizer = new DecisionsHistogram(this);
}

BayesianStrategyEvaluator::~BayesianStrategyEvaluator()
{
    clearDistributions();
    delete likelihood;
    delete normalizer;
}

void
BayesianStrategyEvaluator::clearDistributions()
{
    for (auto& p : estimatorSamples) {
        StatsDistributionBinned *distribution = p.second;
        delete distribution;
    }
    estimatorSamples.clear();
    last_estimator_values.clear();
    ordered_observations.clear();
    num_historical_observations = 0;

    likelihood->clear();
    normalizer->clear();
}

string
BayesianStrategyEvaluator::makeSimpleEvaluatorName()
{
    ostringstream oss;
    oss << getName() << "-simple";
    return oss.str();
}

void
BayesianStrategyEvaluator::setStrategies(const instruments_strategy_t *new_strategies,
                                         size_t num_strategies)
{
    StrategyEvaluator::setStrategies(new_strategies, num_strategies);
    if (!simple_evaluator) {
        simple_evaluator = new SimpleEvaluator(makeSimpleEvaluatorName(), new_strategies, num_strategies);
        simple_evaluator->setSilent(true);
    }

    for (Estimator *estimator : getAllEstimators()) {
        estimators_by_name[estimator->getName()] = estimator;
    }
}

bool
BayesianStrategyEvaluator::uninitializedStrategiesExist()
{
    auto is_uninitialized = [&](Estimator *estimator) {
        return estimatorSamples.count(estimator) == 0;
    };
    auto has_uninitialized_estimators = [&](Strategy *strategy) {
        if (strategy->isRedundant()) {
            // ignore; captured by child strategies' estimators
            return false;
        };

        auto estimators = strategy->getEstimators();
        return any_of(estimators.begin(), estimators.end(), is_uninitialized);
    };
    return any_of(strategies.begin(), strategies.end(), has_uninitialized_estimators);
}

Strategy *
BayesianStrategyEvaluator::getBestSingularStrategy(void *chooser_arg)
{
    ASSERT(!uninitializedStrategiesExist());
    return (Strategy *) simple_evaluator->chooseStrategy(chooser_arg);
}


double 
BayesianStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // it's bandwidth or latency stored in the distribution, rather
    // than an error value, so just return the value.
    // XXX-BAYESIAN:  yes, this may over-emphasize history.  known (potential) issue.

    return likelihood->getCurrentEstimatorSample(estimator);
}

void 
BayesianStrategyEvaluator::processObservation(Estimator *estimator, double observation,
                                              double old_estimate, double new_estimate)
{
    inst::dbgprintf(INFO, "[bayesian] %s evaluator: Adding observation %f to estimator %s\n",
                    getName(), observation, estimator->getName().c_str());

    bool add_decision = !uninitializedStrategiesExist();

    const string& name = estimator->getName();
    if (estimators_by_name.count(name) == 0) {
        estimators_by_name[name] = estimator;
    }
    if (estimatorSamples.count(estimator) == 0) {
        estimatorSamples[estimator] = createStatsDistribution(estimator);
    }
    estimatorSamples[estimator]->addValue(observation);
    
    likelihood->addObservation(estimator, observation);
    if (add_decision) {
        auto estimator_values = simple_evaluator->getEstimatorValues();
        likelihood->addDecision(estimator_values);
        normalizer->addDecision(estimator_values);
    }

    ordered_observations.push_back({estimator->getName(), observation, old_estimate, new_estimate});

    simple_evaluator->setEstimatorValue(estimator, new_estimate);
}

void 
BayesianStrategyEvaluator::processEstimatorReset(Estimator *estimator, const char *filename)
{
    decltype(ordered_observations) replay_observations;
    if (filename) {
        // get all observations since the last restore
        replay_observations.assign(ordered_observations.begin() + num_historical_observations, 
                                   ordered_observations.end());
        
        restoreFromFileImpl(filename);
    } else {
        replay_observations = ordered_observations;
        clearDistributions();
    }
        
    string estimator_name = estimator->getName();
    
    // make sure there's at least one observation for this estimator 
    //  (the first one among the ones we're choosing not to replay)
    bool no_observations = (!filename);
    
    // replay the observations for all other estimators
    for (auto& obs : replay_observations) {
        if (obs.estimator_name != estimator_name || no_observations) {
            Estimator *other_estimator = getEstimator(obs.estimator_name);
            processObservation(other_estimator, 
                               obs.observation, obs.old_estimate, obs.new_estimate);

            if (no_observations && obs.estimator_name == estimator_name) {
                no_observations = false;
            }
        }
    }
}

StatsDistributionBinned *
BayesianStrategyEvaluator::createStatsDistribution(Estimator *estimator)
{
    return StatsDistributionBinned::create(estimator, weighted);
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

combiner_fn_t
get_combiner_fn(typesafe_eval_fn_t fn)
{
    if (fn == redundant_strategy_minimum_time) {
        return min;
    } else if (fn == redundant_strategy_total_energy_cost ||
               fn == redundant_strategy_total_data_cost) {
        return sum;
    } else {
        return NULL;
    }
}

double
BayesianStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                         void *strategy_arg, void *chooser_arg,
                                         ComparisonType comparison_type)
{
    instruments_strategy_t *strategies_array = new instruments_strategy_t[strategies.size()];
    for (size_t i = 0; i < strategies.size(); ++i) {
        strategies_array[i] = strategies[i];
    }

    // temporary, so don't store updates to me
    SimpleEvaluator tmp_simple_evaluator(makeSimpleEvaluatorName(), strategies_array, strategies.size());
    delete [] strategies_array;
    tmp_simple_evaluator.setSilent(true);
    
    return likelihood->getWeightedSum(&tmp_simple_evaluator, strategy, 
                                      fn, strategy_arg, chooser_arg);
}

BayesianStrategyEvaluator::Likelihood::Likelihood(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_)
{
}


void
BayesianStrategyEvaluator::Likelihood::addObservation(Estimator *estimator, double observation)
{
    last_observation[estimator] = observation;
}

void 
BayesianStrategyEvaluator::Likelihood::addDecision(const vector<pair<Estimator *,double> >& estimator_values)
{
    // get the current key over all estimators (vector of bins, based on estimator values) 
    // look up the histogram in the likelihood map
    // add an entry to the decisions pseudo-histogram
    DistributionKey key = getCurrentEstimatorKey(estimator_values);
    if (likelihood_distribution.count(key) == 0) {
        // make sure that all vectors used as keys for the map have the same length
        ASSERT(likelihood_distribution.empty() || 
               (likelihood_distribution.begin()->first == key ||
                !(likelihood_distribution.begin()->first == key)));
        likelihood_distribution[key] = new DecisionsHistogram(evaluator);
    }
    DecisionsHistogram *histogram = likelihood_distribution[key];
    ASSERT(histogram);
    histogram->addDecision(estimator_values);
}

void
BayesianStrategyEvaluator::Likelihood::setEstimatorSamples(DistributionKey& key)
{
    key.forEachEstimator([&](Estimator *estimator, double sample) {
            currentEstimatorSamples[estimator] = sample;
            return true;
        });
}

DistributionKey
BayesianStrategyEvaluator::Likelihood::getCurrentEstimatorKey(const vector<pair<Estimator *, double> >& estimator_values)
{
    DistributionKey key(evaluator);
    for (auto& p : estimator_values) {
        Estimator *estimator = p.first;
        ASSERT(last_observation.count(estimator) > 0);
        
        double obs = last_observation[estimator];
        key.addEstimatorValue(estimator, obs);
    }
    return key;
}

DistributionKey
BayesianStrategyEvaluator::Likelihood::makeEstimatorKey(const vector<pair<Estimator *, double> >& estimator_values)
{
    DistributionKey key(evaluator);
    for (auto& p : estimator_values) {
        Estimator *estimator = p.first;
        double obs = p.second;
        key.addEstimatorValue(estimator, obs);
    }
    return key;
}

double 
BayesianStrategyEvaluator::Likelihood::getCurrentEstimatorSample(Estimator *estimator)
{
    ASSERT(currentEstimatorSamples.count(estimator) > 0);
    return currentEstimatorSamples[estimator];
}


double
BayesianStrategyEvaluator::Likelihood::jointPriorProbability(DistributionKey& key)
{
    double probability = 1.0;
    static ostringstream oss;
    if (inst::is_debugging_on(DEBUG)) {
        oss.str("");
    }
    key.forEachEstimator([&](Estimator *estimator, double sample) {
            ASSERT(evaluator->estimatorSamples.count(estimator) > 0);
            if (!estimator->valueMeetsConditions(sample)) {
                // we're doing a conditional probability summation, so
                // prune this sample from the joint prior distribution
                probability = 0.0;
                return false; // break the foreach iteration
            }
            double single_prob = evaluator->estimatorSamples[estimator]->getProbability(sample);
            probability *= single_prob;
            if (inst::is_debugging_on(DEBUG)) {
                oss << single_prob << " ";
            }
            return true;
        });
    /*
    if (inst::is_debugging_on(DEBUG)) {
        string s = oss.str();
        dbgprintf(DEBUG, "Probabilities: %s  joint: %f\n", s.c_str(), probability);
    }
    */
    return probability;
}

#ifdef NDEBUG
#define assert_valid_probability(value)
#else
#define assert_valid_probability(value)                                 \
    do {                                                                \
        const double threshold = 0.00001;                               \
        if (value < -threshold || value - 1.0 > threshold) {            \
            fprintf(stderr, "%s (%f) is invalid probability at %s:%d\n", \
                    #value, value, __FILE__, __LINE__);                 \
            ASSERT(false);                                              \
        }                                                               \
    } while (0)
#endif

double 
BayesianStrategyEvaluator::Likelihood::getWeightedSum(SimpleEvaluator *tmp_simple_evaluator, 
                                                      Strategy *strategy, typesafe_eval_fn_t fn,
                                                      void *strategy_arg, void *chooser_arg)
{
    double weightedSum = 0.0;
#ifndef NDEBUG
    double prior_sum = 0.0;
#endif
    double posterior_sum = 0.0;

    const auto& estimator_values = evaluator->simple_evaluator->getEstimatorValues();
    DistributionKey cur_key = getCurrentEstimatorKey(estimator_values);

    string value_name = get_value_name(strategy, fn);
    
    bool debugging = inst::is_debugging_on(DEBUG);
    
    if (likelihood_distribution.empty()) {
        inst::dbgprintf(DEBUG, "[bayesian] (no entries in likelihood distribution)\n");
    } else {
        size_t print_size = likelihood_distribution.begin()->first.getPrintSize();
        inst::dbgprintf(DEBUG, "[bayesian] %*s   %13s %10s %10s %10s\n",
                        print_size - 2,  "key", "prior", "likelihood", "posterior", value_name.c_str());
    }

    // Store joint_prior along with decisions histogram
    //  in order to set the single joint probability
    //  in the extreme corner case
    map<DistributionKey, pair<double, DecisionsHistogram *> > pruned_likelihood_distribution;
    for (const LikelihoodMap::reference map_pair : likelihood_distribution) {
        DistributionKey key = map_pair.first;
        DecisionsHistogram *histogram = map_pair.second;
        double joint_prior = jointPriorProbability(key);
        if (joint_prior > 0.0) {
            pruned_likelihood_distribution[key] = make_pair(joint_prior, histogram);
        }
    }
    DecisionsHistogram *dummy_decision = nullptr;
    if (pruned_likelihood_distribution.empty()) {
        // I have no history that helps me decide what to do;
        // all my history is ruled out by the conditional bounds.
        // Therefore, I just return the function call value with the 
        // last estimator values, conditional bounds considered
        // (which is essentially what the other methods are doing in this case too).
        auto dummy_estimator_values = evaluator->simple_evaluator->getEstimatorValues();
        for (auto& p : dummy_estimator_values) {
            Estimator *estimator = p.first;
            double& value = p.second;
            // there may be multiple estimators out of bounds;
            // make sure we get them all
            if (!estimator->valueMeetsConditions(value)) {
                value = estimator->getConditionalBound();
            }
        }
        tmp_simple_evaluator->setEstimatorValues(dummy_estimator_values);
        if (inst::is_debugging_on(inst::DEBUG)) {
            ostringstream s;
            for (auto& p : dummy_estimator_values) {
                s << p.second << " ";
            }
            inst::dbgprintf(inst::DEBUG, "History all pruned by conditional bounds; Just using estimator values: %s\n", 
                            s.str().c_str());
        }
        return tmp_simple_evaluator->expectedValue(strategy, fn, strategy_arg, chooser_arg, COMPARISON_TYPE_IRRELEVANT);
    }

    Stopwatch stopwatch;
    stopwatch.setEnabled(debugging);
    
    for (auto& map_pair : pruned_likelihood_distribution) {
        DistributionKey key = map_pair.first;
        auto& value_pair = map_pair.second;
        double joint_prior = value_pair.first;
        DecisionsHistogram *histogram = value_pair.second;

        stopwatch.start("setEstimatorSamples");
        setEstimatorSamples(key);
        stopwatch.start("eval_fn");
        double value = fn(evaluator, strategy_arg, chooser_arg);
        stopwatch.stop();
        double prior = joint_prior;
        assert_valid_probability(prior);

#ifndef NDEBUG
        prior_sum += prior;
        assert_valid_probability(prior_sum);
#endif
    
        bool ensure_nonzero = (cur_key == key);
        stopwatch.start("likelihood");
        double likelihood_coeff = histogram->getWinnerProbability(tmp_simple_evaluator, chooser_arg, ensure_nonzero);
        stopwatch.stop();
        if (likelihood_coeff == 0.0) {
            stopwatch.start("posterior");
            stopwatch.start("printing");
            stopwatch.start("remaining summation");
            stopwatch.stop();
            stopwatch.freezeLabels();
            continue;
        }
        stopwatch.start("posterior");
        double posterior = prior * likelihood_coeff;
        stopwatch.stop();

        assert_valid_probability(likelihood_coeff);
        assert_valid_probability(posterior);
        
        stopwatch.start("printing");
        if (debugging) {
            ostringstream s;
            s << "[bayesian] " << key
              << " " << setw(13) << prior 
              << " " << setw(10) << likelihood_coeff
              << " " << setw(10) << posterior
              << " " << setw(10) << value;
            inst::dbgprintf(DEBUG, "%s\n", s.str().c_str());
        }

        stopwatch.start("remaining summation");
        posterior_sum += posterior;
        assert_valid_probability(posterior_sum);
        
        weightedSum += value * posterior;
        stopwatch.stop();

        stopwatch.freezeLabels();
    }

    if (debugging) {
        inst::dbgprintf(DEBUG, "[bayesian] calc times: [ %s ]\n", 
                        stopwatch.toString().c_str());
    }

    delete dummy_decision;
    
    inst::dbgprintf(INFO, "[bayesian] calculated posterior from %zu samples\n", 
                    pruned_likelihood_distribution.size());
#ifndef NDEBUG
    inst::dbgprintf(INFO, "[bayesian] prior sum: %f\n", prior_sum);
#endif
    inst::dbgprintf(INFO, "[bayesian] posterior sum: %f\n", posterior_sum);
    
    // here's the normalization.  summing the posterior values ensures that
    //  I'm using the correct value.
    ASSERT(posterior_sum > 0.0);
    return weightedSum / posterior_sum;
}

void
BayesianStrategyEvaluator::Likelihood::clear()
{
    last_observation.clear();
    currentEstimatorSamples.clear();
    for (auto& q : likelihood_distribution) {
        DecisionsHistogram *histogram = q.second;
        delete histogram;
    }
    likelihood_distribution.clear();
}

BayesianStrategyEvaluator::DecisionsHistogram::
DecisionsHistogram(BayesianStrategyEvaluator *evaluator_)
    : evaluator(evaluator_), last_chooser_arg(NULL)
{
}

void
BayesianStrategyEvaluator::DecisionsHistogram::clear()
{
    decisions.clear();
}

void
BayesianStrategyEvaluator::DecisionsHistogram::
addDecision(const vector<pair<Estimator *, double> >& estimator_values)
{
    decisions.push_back(estimator_values);
    last_winner_probability.clear(); // needs to be recalculated
    // TODO: can do better.  calculate this value every time a decision is added.
    // TODO: calculating it is very cheap, too (don't need to recompute everything
    // TODO:  until the chooser_arg changes)
}

double
BayesianStrategyEvaluator::DecisionsHistogram::getWinnerProbability(SimpleEvaluator *tmp_simple_evaluator,
                                                                    void *chooser_arg, bool ensure_nonzero)
{
    bool debugging = inst::is_debugging_on(DEBUG);
    Stopwatch stopwatch;
    stopwatch.setEnabled(debugging);
    
    Strategy *winner = evaluator->getBestSingularStrategy(chooser_arg);
    ASSERT(winner);

    if (chooser_arg == last_chooser_arg) {
        if (last_winner_probability.count(winner) > 0) {
            return last_winner_probability[winner];
        }
    } else {
        last_winner_probability.clear();
    }

    size_t cur_wins = 0;
    size_t cur_decisions = decisions.size();
    for (auto decision : decisions) {
        stopwatch.start("setEstimatorValues");
        tmp_simple_evaluator->setEstimatorValues(decision);
        stopwatch.start("chooseStrategy");
        Strategy *cur_winner = (Strategy *) tmp_simple_evaluator->chooseStrategy(chooser_arg);
        stopwatch.start("summation");
        if (cur_winner == winner) {
            cur_wins++;
        }
        stopwatch.stop();
        stopwatch.freezeLabels();
    }
    if (debugging) {
        inst::dbgprintf(DEBUG, "[bayesian] winner calc times: [ %s ]\n", 
                        stopwatch.toString().c_str());
    }

    if (cur_wins == 0 && ensure_nonzero) {
        ++cur_wins;
        ++cur_decisions;
    }
    double prob = cur_wins / ((double) cur_decisions);
    last_chooser_arg = chooser_arg;
    last_winner_probability[winner] = prob;
    return prob;
}


static int PRECISION = 20;

static void write_estimate(ostream& out, double estimate)
{
    if (estimate_is_valid(estimate)) {
        out << estimate << " ";
    } else {
        out << "(invalid) ";
    }
}

static std::istream&
read_estimate(std::istream& in, double& estimate)
{
    string estimate_str;
    if (in >> estimate_str) {
        istringstream s(estimate_str);
        if (!(s >> estimate)) {
            estimate = invalid_estimate();
        }
    }
    return in;
}

void
BayesianStrategyEvaluator::saveToFile(const char *filename)
{
    ostringstream err("Failed to open ");
    err << filename;

    ofstream out(filename);
    check(out, err.str());

    out << estimators_by_name.size() << " estimators" << endl;
    out << "name hint_min hint_max hint_num_bins" << endl;
    for (auto& p : estimators_by_name) {
        const string& name = p.first;
        Estimator *estimator = p.second;
        out << name << " ";
        if (estimator->hasRangeHints()) {
            EstimatorRangeHints hints = estimator->getRangeHints();
            out << hints.min << " " << hints.max << " " << hints.num_bins;
        } else {
            out << "none none none";
        }
        out << endl;
    }
    out << endl;

    out << ordered_observations.size() << " observations" << endl;
    out << "name observation old_estimate new_estimate" << endl;
    for (const stored_observation& obs : ordered_observations) {
        out << obs.estimator_name << " " << setprecision(PRECISION) 
            << obs.observation << " ";
        write_estimate(out, obs.old_estimate);
        write_estimate(out, obs.new_estimate);
        out << endl;

        check(out, "Failed to save bayesian evaluator to file");
    }
    out.close();
}

void
BayesianStrategyEvaluator::restoreFromFileImpl(const char *filename)
{
    inst::dbgprintf(INFO, "Restoring Bayesian distribution from %s\n", filename);

    clearDistributions();
    
    ostringstream err("Failed to open ");
    err << filename;
        
    ifstream in(filename);
    check(in, err.str());

    size_t i = 0;
    size_t num_observations;
    string name;
    double old_estimate;
    double new_estimate;
    double observation;
    string header;

    size_t num_estimators;
    check(in >> num_estimators >> header, "Failed to read num_estimators");
    check(getline(in, header), "Unexpected EOF"); // consume newline
    check(getline(in, header), "Failed to read estimators-header");
    for (i = 0; i < num_estimators; ++i) {
        string line;
        check(getline(in, line), "Failed to get an estimator's hints or lack thereof");

        istringstream iss(line);
        check(iss >> name, "Failed to get estimator name");

        EstimatorRangeHints hints;
        Estimator *estimator = getEstimator(name);
        if (iss >> hints.min >> hints.max >> hints.num_bins) {
            estimator->setRangeHints(hints.min, hints.max, hints.num_bins);
        } // else: no hints, ignore line
    }
    check(getline(in, header), "Missing blank line"); // ignore blank line
    
    check(in >> num_observations >> header, "Failed to read num_observations");
    check(getline(in, header), "Unexpected EOF"); // consume newline
    check(getline(in, header), "Failed to read header"); // ignore header
    i = 0;
    while ((in >> name >> observation) && 
           read_estimate(in, old_estimate) &&
           read_estimate(in, new_estimate)) {
        Estimator *estimator = getEstimator(name);
        processObservation(estimator, observation, old_estimate, new_estimate);
        ++i;
    }
    in.close();
    check(num_observations == i, "Got wrong number of observations");
    
    num_historical_observations = ordered_observations.size();
}

Estimator *
BayesianStrategyEvaluator::getEstimator(const string& name)
{
    ASSERT(estimators_by_name.count(name) > 0);
    return estimators_by_name[name];
}

BayesianStrategyEvaluator::SimpleEvaluator::SimpleEvaluator(const string& name,
                                                            const instruments_strategy_t *new_strategies,
                                                            size_t num_strategies)
    : StrategyEvaluator(true) // these don't need updates from estimators
{
    setName(name.c_str());
    
    instruments_strategy_t *singular_strategies = new instruments_strategy_t[num_strategies];
    size_t count = 0;
    for (size_t i = 0; i < num_strategies; ++i) {
        Strategy *strategy = (Strategy*) new_strategies[i];
        if (!strategy->isRedundant()) {
            singular_strategies[count++] = new_strategies[i];
        }
    }
    StrategyEvaluator::setStrategies(singular_strategies, count);
    delete [] singular_strategies;

    inst::dbgprintf(INFO, "[bayesian] %s evaluator: Creating simple evaluator %p\n",
                    getName(), this);
}

BayesianStrategyEvaluator::SimpleEvaluator::~SimpleEvaluator()
{
    inst::dbgprintf(INFO, "[bayesian] %s evaluator: Destroying simple evaluator %p\n",
                    getName(), this);
}

Strategy *
BayesianStrategyEvaluator::SimpleEvaluator::chooseStrategy(void *chooser_arg)
{
    double min_time = DBL_MAX;
    Strategy *winner = NULL;
    for (Strategy *strategy : strategies) {
        double time = strategy->calculateTime(this, chooser_arg, COMPARISON_TYPE_IRRELEVANT);
        if (!winner || time < min_time) {
            winner = strategy;
            min_time = time;
        }
    }
    return winner;
}

double 
BayesianStrategyEvaluator::SimpleEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn,
                                                          void *strategy_arg, void *chooser_arg,
                                                          ComparisonType comparison_type)
{
    return fn(this, strategy_arg, chooser_arg);
}
 
double 
BayesianStrategyEvaluator::SimpleEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    if (estimator_values.count(estimator) > 0) {
        return estimator_values[estimator];
    } else {
        // this should only happen once, when I have no history on this estimator.
        return estimator->getEstimate();
    }
}


void
BayesianStrategyEvaluator::SimpleEvaluator::setEstimatorValue(Estimator *estimator, double value)
{
    estimator_values[estimator] = value;
}

void
BayesianStrategyEvaluator::SimpleEvaluator::setEstimatorValues(const vector<pair<Estimator *, double> >& new_estimator_values)
{
    for (auto p : new_estimator_values) {
        Estimator *estimator = p.first;
        double value = p.second;
        setEstimatorValue(estimator, value);
    }
}

vector<pair<Estimator *, double> >
BayesianStrategyEvaluator::SimpleEvaluator::getEstimatorValues()
{
    vector<pair<Estimator *, double> > estimator_value_pairs;
    for (auto p : estimator_values) {
        Estimator *estimator = p.first;
        double value = p.second;
        estimator_value_pairs.push_back(make_pair(estimator, value));
    }
    return estimator_value_pairs;
}
