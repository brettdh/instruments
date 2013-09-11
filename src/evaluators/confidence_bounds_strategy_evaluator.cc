#include "confidence_bounds_strategy_evaluator.h"
#include "flipflop_estimate.h"

#include "estimator.h"
#include "error_calculation.h"
#include "debug.h"
namespace inst = instruments;
using inst::INFO; using inst::DEBUG;

#include <math.h>
#include <assert.h>
#include <float.h>

#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <deque>
#include <iomanip>
#include <map>
#include <functional>
#include <algorithm>
#include <vector>
#include <tuple>
using std::ifstream; using std::ofstream;
using std::ostringstream; using std::runtime_error;
using std::string; using std::setprecision; using std::endl;
using std::tuple; using std::make_tuple; using std::min;
using std::deque; using std::function; using std::vector;
using std::copy_if;

#include "students_t.h"

class ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds {
  public:
    ErrorConfidenceBounds(bool weighted_, Estimator *estimator_);
    void processObservation(double observation, double old_estimate, double new_estimate);
    double getBound(BoundType type);
    void saveToFile(ofstream& out);
    string restoreFromFile(ifstream& in);
    string getName();
    void setConditionalBounds();
    void clearConditionalBounds();

    void setEstimator(Estimator *estimator_);
  private:
    static const double CONFIDENCE_ALPHA;

    Estimator *estimator;

    // let's try making these log-transformed.
    double log_error_mean;
    double log_error_variance;
    double M2; // for running variance algorithm
    
    size_t num_samples;

    // only used for conditional probability calculations.
    deque<double> log_error_samples;
    deque<double> smoothed_log_error_samples;
    
    // these are not log-transformed; they are inverse-transformed
    //  before being stored.
    double error_bounds[UPPER + 1];

    double getBoundDistance(double cur_log_error_variance,
                            size_t cur_num_samples);
    void setBounds(double cur_log_error_mean, 
                   double cur_log_error_variance,
                   size_t cur_num_samples);
    void updateErrorDistribution(double log_error);
    void updateErrorDistributionLinear(double log_error);
    void updateErrorDistributionEWMA(double log_error);

    void setConditionalBoundsWhere(function<bool(double)> shouldIncludeSample);

    bool weighted;
    
    FlipFlopEstimate flipflop_log_error;
};


const double ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::CONFIDENCE_ALPHA = 0.05;

ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::ErrorConfidenceBounds(bool weighted_, Estimator *estimator_)
    : estimator(estimator_), weighted(weighted_), flipflop_log_error(estimator ? estimator->getName() : "(no name)")
{
    log_error_mean = 0.0;
    log_error_variance = 0.0;
    M2 = 0.0;
    num_samples = 0;
}

void 
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::setEstimator(Estimator *estimator_)
{
    estimator = estimator_;
}

string 
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::getName()
{
    if (estimator) {
        return estimator->getName();
    }
    
    ostringstream s;
    s << this;
    return s.str();
}


//#define CHEBYSHEV_BOUNDS
//#define CONFIDENCE_INTERVAL_BOUNDS
#define PREDICTION_INTERVAL_BOUNDS

#if defined(CHEBYSHEV_BOUNDS)
static double get_chebyshev_bound(double alpha, double variance)
{
    // Chebyshev interval for error bounds, as described in my proposal.
    return sqrt(variance / alpha);
}
#define GET_BOUND_DISTANCE(alpha, variance, num_samples) get_chebyshev_bound(alpha, variance)

#elif defined(CONFIDENCE_INTERVAL_BOUNDS)
static double get_confidence_interval_width(double alpha, double variance, size_t num_samples)
{
    // student's-t-based confidence interval
    if (num_samples <= 1) {
        return 0.0;
    } else {
        double t = get_t_value(alpha, num_samples - 1);
        return t * sqrt(variance / num_samples);
    }
}
#define GET_BOUND_DISTANCE(alpha, variance, num_samples) get_confidence_interval_width(alpha, variance, num_samples)

#elif defined(PREDICTION_INTERVAL_BOUNDS)

static double get_prediction_interval_width(double alpha, double variance, size_t num_samples)
{
    // student's-t-based prediction interval
    // http://en.wikipedia.org/wiki/Prediction_interval#Unknown_mean.2C_unknown_variance
    if (num_samples <= 1) {
        return 0.0;
    } else {
        double t = get_t_value(alpha, num_samples - 1);
        return t * sqrt(variance * (1 + (1 / num_samples)));
    }
}
#define GET_BOUND_DISTANCE(alpha, variance, num_samples) get_prediction_interval_width(alpha, variance, num_samples)

#else
#error "Must define a bounds calcuation method."
#endif

double
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::getBoundDistance(double variance, size_t num_samples)
{
    return GET_BOUND_DISTANCE(CONFIDENCE_ALPHA, variance, num_samples);
}

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::updateErrorDistribution(double log_error)
{
    if (weighted) {
        updateErrorDistributionEWMA(log_error);
    } else {
        updateErrorDistributionLinear(log_error);
    }
}

static void
update_error_distribution_linear(size_t& num_samples,
                                 double& log_error_mean,
                                 double& log_error_variance,
                                 double& M2,
                                 double log_error)
{
    /*
     * Algorithm borrowed from
     * http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm
     * which cites Knuth's /The Art of Computer Programming/, Volume 1.
     */
    ++num_samples;
    double delta = log_error - log_error_mean;
    log_error_mean += delta / num_samples;
    M2 += delta * (log_error - log_error_mean);
    if (num_samples > 1) {
        log_error_variance = M2 / (num_samples-1);
    }
}

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::updateErrorDistributionLinear(double log_error)
{
    flipflop_log_error.add_observation(log_error);
    double smoothed_log_error;
    if (!flipflop_log_error.get_estimate(smoothed_log_error)) {
        ASSERT(false); // I just added a value, so the estimate must be valid.
    }

    update_error_distribution_linear(num_samples, log_error_mean, log_error_variance, M2, smoothed_log_error);
    log_error_samples.push_back(log_error);
    smoothed_log_error_samples.push_back(smoothed_log_error);
}

#include "error_weight_params.h"
using instruments::MAX_SAMPLES;
using instruments::NEW_SAMPLE_WEIGHT;
using instruments::EWMA_GAIN;
using instruments::update_ewma;

static void
update_error_distribution_ewma(size_t& num_samples,
                               double& log_error_mean,
                               double& log_error_variance,
                               double log_error)
{
    num_samples = min(MAX_SAMPLES, num_samples + 1);
    if (num_samples == 1) {
        log_error_mean = log_error;
        log_error_variance = 0.0;
    } else {
        double deviation = log_error_mean - log_error;

        update_ewma(log_error_mean, log_error, EWMA_GAIN);
        update_ewma(log_error_variance, deviation * deviation, EWMA_GAIN);
    }
    
}

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::updateErrorDistributionEWMA(double log_error)
{
    flipflop_log_error.add_observation(log_error);
    double smoothed_log_error;
    if (!flipflop_log_error.get_estimate(smoothed_log_error)) {
        ASSERT(false); // I just added a value, so the estimate must be valid.
    }
    
    update_error_distribution_ewma(num_samples, log_error_mean, log_error_variance, smoothed_log_error);
    
    // keep the original unsmoothed sample so I can reconstruct the flipflop value later
    //  (e.g. for save/restore)
    log_error_samples.push_back(log_error);
    smoothed_log_error_samples.push_back(smoothed_log_error);
    ASSERT(log_error_samples.size() == smoothed_log_error_samples.size());
    if (log_error_samples.size() > num_samples) {
        assert(num_samples + 1 == log_error_samples.size());
        log_error_samples.pop_front();
        smoothed_log_error_samples.pop_front();
    }
}

void 
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::
setConditionalBounds()
{
    auto shouldIncludeSample = [=](double log_error) {
        double adjusted_value = adjusted_estimate(estimator->getEstimate(), 
                                                  exp(log_error));
        return estimator->valueMeetsConditions(adjusted_value);
    };
    setConditionalBoundsWhere(shouldIncludeSample);
}

void 
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::
setConditionalBoundsWhere(function<bool(double)> shouldIncludeSample)
{
    double cur_log_error_mean = 0.0, cur_log_error_variance = 0.0;
    double cur_M2 = 0.0;
    size_t cur_num_samples = 0;

    ostringstream s;
    bool debugging = inst::is_debugging_on(DEBUG);
    vector<double> pruned_samples;
    pruned_samples.reserve(smoothed_log_error_samples.size());
    for (double log_error_sample : smoothed_log_error_samples) {
        if (shouldIncludeSample(log_error_sample)) {
            if (debugging) {
                s << exp(log_error_sample) << " ";
            }
            pruned_samples.push_back(log_error_sample);
        }
    }
    if (pruned_samples.empty()) {
        // see comments in ../joint_distributions/intnw_joint_distribution.cc
        // for why I'm doing this step
        if (!estimator->hasConditions()) {
            // we really just have no error samples yet.
            return;
        }
        double value = estimator->getConditionalBound();
        double error = calculate_error(estimator->getEstimate(), value);
        double log_error = log(error);
        pruned_samples.push_back(log_error);
        if (debugging) {
            s << error << " ";
        }
    }
    
    dbgprintf(DEBUG, "Estimator %s Using error samples: [ %s]\n", 
              estimator->getName().c_str(), s.str().c_str());
    for (double log_error : pruned_samples) {
        if (weighted) {
            update_error_distribution_ewma(cur_num_samples,
                                           cur_log_error_mean,
                                           cur_log_error_variance,
                                           log_error);
        } else {
            update_error_distribution_linear(cur_num_samples,
                                             cur_log_error_mean,
                                             cur_log_error_variance,
                                             cur_M2,
                                             log_error);
        }
    }
    setBounds(cur_log_error_mean, cur_log_error_variance, cur_num_samples);
}

void 
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::clearConditionalBounds()
{
    setConditionalBoundsWhere([](double) { return true; });
}


void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::processObservation(double observation, 
                                                                             double old_estimate,
                                                                             double new_estimate)
{
    string name = estimator->getName();
    inst::dbgprintf(INFO, "Getting error sample from estimator %s\n", name.c_str());

    double error = calculate_error(old_estimate, observation);
    double log_error = log(error); // natural logarithm

    updateErrorDistribution(log_error);
    inst::dbgprintf(INFO, "Adding error sample to estimator %s: %f\n",
                    name.c_str(), error);

    setBounds(log_error_mean, log_error_variance, num_samples);
    
    if (adjusted_estimate(old_estimate, error_bounds[LOWER]) < 0.0) {
        // PROBLEMATIC.
        ASSERT(false); // TODO: figure out what to do here.
    }
}

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::setBounds(double cur_log_error_mean, 
                                                                    double cur_log_error_variance,
                                                                    size_t cur_num_samples)
{
    double bound_distance = getBoundDistance(cur_log_error_variance, cur_num_samples);
    double bounds[2];

    // just need some value so that the sorting of {lower,upper} bounds
    // is independent of whether it's absolute or relative error.
    const double ref_value = 100.0;

    bounds[0] = exp(cur_log_error_mean - bound_distance);
    bounds[1] = exp(cur_log_error_mean + bound_distance);
    if (adjusted_estimate(ref_value, bounds[0]) < adjusted_estimate(ref_value, bounds[1])) {
        error_bounds[LOWER] = bounds[0];
        error_bounds[UPPER] = bounds[1];
    } else {
        error_bounds[LOWER] = bounds[1];
        error_bounds[UPPER] = bounds[0];
    }
    inst::dbgprintf(INFO, "Estimator %s  n=%4zu; error bounds: [%f, %f]\n", 
                    estimator->getName().c_str(), cur_num_samples, 
                    error_bounds[LOWER], error_bounds[UPPER]);
}


double
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::getBound(BoundType type)
{
    double error;
    if (type == CENTER) {
        error = error_midpoint(error_bounds[LOWER], error_bounds[UPPER]);
    } else {
        error = num_samples > 0 ? error_bounds[type] : no_error_value();
    }
    return adjusted_estimate(estimator->getEstimate(), error);
}

static int PRECISION = 20;

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::saveToFile(ofstream& out)
{
    out << estimator->getName() << " " << setprecision(PRECISION)
        << "num_samples " << num_samples
        << " mean " << log_error_mean 
        << " variance " << log_error_variance
        << " M2 " << M2 
        << " bounds " << error_bounds[LOWER] << " " << error_bounds[UPPER]
        << " samples ";
    for (double sample : log_error_samples) {
        out << sample << " ";
    }
    out << endl;
}

string
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::restoreFromFile(ifstream& in)
{
    string name, tag;
    in >> name >> tag >> num_samples;
    check(tag == "num_samples", "Failed to parse num_samples");
    
    typedef struct {
        const char *expected_tag;
        double& field;
    } field_t;
    field_t fields[] = {
        { "mean", log_error_mean },
        { "variance", log_error_variance },
        { "M2", M2 },
        { "bounds", error_bounds[LOWER] }
    };
    size_t num_fields = sizeof(fields) / sizeof(field_t);
    for (size_t i = 0; i < num_fields; ++i) {
        in >> tag >> fields[i].field;
        check(tag == fields[i].expected_tag, "Failed to parse field");
    }
    in >> error_bounds[UPPER];
    check(in, "Failed to read bounds from file");

    flipflop_log_error.reset(0.0);

    in >> tag;
    check(tag == "samples", "Failed to read 'samples' tag from file");
    for (size_t i = 0; i < num_samples; ++i) {
        double sample;
        in >> sample;
        check(in, "Failed to read sample from file");
        log_error_samples.push_back(sample);
        flipflop_log_error.add_observation(sample);

        double smoothed_sample;
        if (!flipflop_log_error.get_estimate(smoothed_sample)) {
            ASSERT(false);
        }
        smoothed_log_error_samples.push_back(smoothed_sample);
    }
    // if weighted, only the max number of samples will have been saved
    return name;
}

void 
ConfidenceBoundsStrategyEvaluator::processObservation(Estimator *estimator, double observation, 
                                                      double old_estimate, double new_estimate)
{
    inst::dbgprintf(INFO, "Adding observation %f to estimator %s\n", 
                    observation, estimator->getName().c_str());
    
    assert(estimator);
    string name = estimator->getName();
    ErrorConfidenceBounds *bounds = NULL;
    if (bounds_by_estimator.count(estimator) == 0) {
        if (placeholders.count(name) > 0) {
            bounds = placeholders[name];
            bounds->setEstimator(estimator);
            placeholders.erase(name);
        } else {
            bounds = new ErrorConfidenceBounds(weighted, estimator);
        }
        error_bounds.push_back(bounds);
        bounds_by_estimator[estimator] = error_bounds.back();
    } else {
        bounds = bounds_by_estimator[estimator];
    }
    
    if (estimators_by_name.count(name) > 0) {
        assert(estimator == estimators_by_name[name]);
    } else {
        estimators_by_name[name] = estimator;
    }
    
    if (estimate_is_valid(old_estimate)) {
        inst::dbgprintf(INFO, "Adding observation %f to estimator-bounds %s\n",
                        observation, bounds->getName().c_str());
        bounds->processObservation(observation, old_estimate, new_estimate);
    }

    clearCache();
}

void 
ConfidenceBoundsStrategyEvaluator::processEstimatorConditionsChange(Estimator *estimator)
{
    clearCache();
}


ConfidenceBoundsStrategyEvaluator::EvalMode
ConfidenceBoundsStrategyEvaluator::DEFAULT_EVAL_MODE = AGGRESSIVE;

static int CENTER_OF_BOUNDS = -1;

ConfidenceBoundsStrategyEvaluator::ConfidenceBoundsStrategyEvaluator(bool weighted_)
    : eval_mode(DEFAULT_EVAL_MODE), // TODO: set as option?
      step(CENTER_OF_BOUNDS), last_chooser_arg(NULL), weighted(weighted_)
{
}

ConfidenceBoundsStrategyEvaluator::~ConfidenceBoundsStrategyEvaluator()
{
    for (size_t i = 0; i < error_bounds.size(); ++i) {
        delete error_bounds[i];
    }
    error_bounds.clear();
    bounds_by_estimator.clear();
    estimators_by_name.clear();
    placeholders.clear();
}

ConfidenceBoundsStrategyEvaluator::BoundType
ConfidenceBoundsStrategyEvaluator::getBoundType(EvalMode eval_mode, Strategy *strategy, 
                                                typesafe_eval_fn_t fn, ComparisonType comparison_type)
{
    if (eval_mode == AGGRESSIVE) {
        // aggressive = upper bound on benefit of redundancy.
        //            = max_time(singular) - min_time(redundant)
        // or:
        // //aggressive = lower bound on additional cost of redundancy
        // //           = min_cost(redundant) - max_cost(singular)
        // //same bound for time and cost.
        // PROBLEM WITH THAT: cost diff can now be negative.
        //    could lower-bound at 0, but:
        //    a) This actually isn't what I proposed; and
        //    b) This will probably lead to always-redundant, all the time.
        //    What I proposed was that min-additional cost is 
        //      calculated using the min-cost for all singular strategies.
        //    So, let's try that.
        //return strategy->isRedundant() ? LOWER : UPPER;

        if (fn == strategy->getEvalFn(ENERGY_FN) ||
            fn == strategy->getEvalFn(DATA_FN)) {
            return LOWER;
        } else {
            if (strategy->isRedundant()) {
                return LOWER;
            } else if (comparison_type == SINGULAR_TO_SINGULAR) {
                return CENTER;
            } else {
                // singular strategy, comparing to redundant.  return upper bound.
                return UPPER;
            }
        }
    } else {
        assert(eval_mode == CONSERVATIVE);
        // XXX: can these be negative? e.g. is it possible that min_time(singular) < max_time(redundant)?
        // XXX: above, the calculation of net benefit is:
        // XXX:  (best_singular_time - best_redundant_time) - (best_redundant_cost - best_singular_cost)
        // XXX: ...where 'best' always means in terms of time.
        // XXX: and then the redundant strategy is chosen if net_benefit > 0.0.
        // XXX: so, if min_time(singular) < max_time(redundant), we expect no benefit from redundancy.
        // XXX: net benefit will be < 0.0, and the singular strategy will be chosen.
        // XXX: I need to think about this more; I'm not sure yet how much it makes sense.
        
        // conservative = lower bound on benefit of redundancy
        //              = min_time(singular) - max_time(redundant)
        // or:
        // conservative = upper bound on cost of redundancy
        //              = max_cost(redundant) - min_cost(singular)
        // same bound for time and cost.
        return strategy->isRedundant() ? UPPER : LOWER;
    }
}

double
ConfidenceBoundsStrategyEvaluator::expectedValue(Strategy *strategy, typesafe_eval_fn_t fn, 
                                                 void *strategy_arg, void *chooser_arg,
                                                 ComparisonType comparison_type)
{
    if (chooser_arg != last_chooser_arg) {
        clearCache();
    }
    last_chooser_arg = chooser_arg;

    auto key = make_tuple(strategy, fn, comparison_type);
    if (cache.count(key) == 0) {
        BoundType bound_type = getBoundType(eval_mode, strategy, fn, comparison_type);
        cache[key] = evaluateBounded(bound_type, fn, strategy_arg, chooser_arg);
    }

    return cache[key];
}

typedef const double& (*bound_fn_t)(const double&, const double&);

double
ConfidenceBoundsStrategyEvaluator::evaluateBounded(BoundType bound_type, typesafe_eval_fn_t fn,
                                                   void *strategy_arg, void *chooser_arg)
{
    double val;
    setConditionalBounds();

    if (bound_type == CENTER) {
        step = CENTER_OF_BOUNDS;
        val = fn(this, strategy_arg, chooser_arg);
        inst::dbgprintf(inst::DEBUG, "Used center of bounds; value is %f\n", val);
    } else {
        bound_fn_t bound_fns[UPPER + 1] = { &std::min<double>, &std::max<double> };
        bound_fn_t bound_fn = bound_fns[bound_type];

        // opposite fn, for calculating initial value that will
        //  always be replaced
        bound_fn_t bound_init_fn = bound_fns[(bound_type + 1) % (UPPER + 1)];

        // yes, it's a 2^N algorithm, but N is small; e.g. 4 for intnw
        int finish = 1 << error_bounds.size();
        val = bound_init_fn(0.0, std::numeric_limits<double>::max());
        ostringstream s;
        bool debugging = inst::is_debugging_on(inst::DEBUG);
        for (step = 0; step < finish; ++step) {
            double cur_val = fn(this, strategy_arg, chooser_arg);
            val = bound_fn(val, cur_val);
            if (debugging) {
                s << cur_val << " ";
            }
        }
        inst::dbgprintf(inst::DEBUG, "%s is %f; values: [ %s]\n",
                        bound_type == LOWER ? "min" : "max",
                        val, s.str().c_str());
    }
    clearConditionalBounds();

    return val;
}

void
ConfidenceBoundsStrategyEvaluator::setConditionalBounds()
{
    for (ErrorConfidenceBounds *bounds : error_bounds) {
        bounds->setConditionalBounds();
    }
}

void
ConfidenceBoundsStrategyEvaluator::clearConditionalBounds()
{
    for (ErrorConfidenceBounds *bounds : error_bounds) {
        bounds->clearConditionalBounds();
    }
}

double
ConfidenceBoundsStrategyEvaluator::getAdjustedEstimatorValue(Estimator *estimator)
{
    // use the low bits of step as selectors for the array of bounds
    //  (i.e. 0 bit = LOWER, 1 bit = UPPER)
    // LSB is used for bounds[0], MSB for bounds[n-1]
    // makes the N-way nested loop simpler
    
    if (bounds_by_estimator.count(estimator) == 0) {
        return estimator->getEstimate();
    }

    ErrorConfidenceBounds *est_error = bounds_by_estimator[estimator];
    for (size_t i = 0; i < error_bounds.size(); ++i) {
        if (est_error == error_bounds[i]) {
            if (step == CENTER_OF_BOUNDS) {
                return est_error->getBound(CENTER);
            } else {
                char bit = (step >> i) & 0x1;
                return est_error->getBound(BoundType(bit));
            }
        }
    }
    __builtin_unreachable();
}

void
ConfidenceBoundsStrategyEvaluator::saveToFile(const char *filename)
{
    ofstream out(filename);
    if (!out) {
        ostringstream oss;
        oss << "Failed to open " << filename;
        throw runtime_error(oss.str());
    }

    out << bounds_by_estimator.size() << " estimator-bounds" << endl;
    for (size_t i = 0; i < error_bounds.size(); ++i) {
        error_bounds[i]->saveToFile(out);
    }
    out.close();
}

void 
ConfidenceBoundsStrategyEvaluator::restoreFromFileImpl(const char *filename)
{
    ifstream in(filename);
    if (!in) {
        ostringstream oss;
        oss << "Failed to open " << filename;
        throw runtime_error(oss.str());
    }

    string tag;
    int num_estimators = -1;
    in >> num_estimators >> tag;
    check(num_estimators >= 0, "Failed to read number of estimator bounds");
    check(tag == "estimator-bounds", "Failed to read tag");

    for (size_t i = 0; i < error_bounds.size(); ++i) {
        delete error_bounds[i];
    }
    error_bounds.clear();
    
    for (int i = 0; i < num_estimators; ++i) {
        ErrorConfidenceBounds *bounds = new ErrorConfidenceBounds(weighted, NULL);
        string name = bounds->restoreFromFile(in);
        if (estimators_by_name.count(name) > 0) {
            Estimator *estimator = estimators_by_name[name];
            bounds->setEstimator(estimator);
            error_bounds.push_back(bounds);
            bounds_by_estimator[estimator] = bounds;
        } else {
            assert(placeholders.count(name) == 0);
            placeholders[name] = bounds;
        }
    }
    
    in.close();

    clearCache();
}

void
ConfidenceBoundsStrategyEvaluator::clearCache()
{
    cache.clear();
}

bool 
ConfidenceBoundsStrategyEvaluator::singularComparisonIsDifferent()
{
    return true;
}
