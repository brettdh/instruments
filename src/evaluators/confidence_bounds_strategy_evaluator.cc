#include "confidence_bounds_strategy_evaluator.h"
#include "estimator.h"
#include "error_calculation.h"
#include "debug.h"

#include <math.h>
#include <assert.h>

#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
using std::ifstream; using std::ofstream;
using std::ostringstream; using std::runtime_error;

#include <boost/math/distributions/students_t.hpp>
using namespace boost::math;

class ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds {
  public:
    ErrorConfidenceBounds(Estimator *estimator_);
    void observationAdded(double value);
    double getBound(BoundType type);

  private:
    static const double CONFIDENCE_ALPHA;

    Estimator *estimator;

    // let's try making these log-transformed.
    double error_mean;
    double error_variance;
    double M2; // for running variance algorithm
    
    size_t num_samples;
    
    // these are not log-transformed; they are inverse-transformed
    //  before being stored.
    double error_bounds[UPPER + 1];

    double getBoundDistance();
};


const double ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::CONFIDENCE_ALPHA = 0.05;

ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::ErrorConfidenceBounds(Estimator *estimator_)
    : estimator(estimator_), error_mean(0.0), error_variance(0.0), M2(0.0), num_samples(0)
{
}

static double get_chebyshev_bound(double alpha, double variance)
{
    // Chebyshev interval for error bounds, as described in my proposal.
    return sqrt(variance / alpha);
}

static double get_t_value(double alpha, size_t df)
{
    //http://www.boost.org/doc/libs/1_39_0/libs/math/doc/sf_and_dist/html/
    //       math_toolkit/dist/stat_tut/weg/st_eg/tut_mean_intervals.html
    return quantile(complement(students_t(df), alpha / 2));
}

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

double
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::getBoundDistance()
{
    if (false) {
        return get_chebyshev_bound(CONFIDENCE_ALPHA, error_variance);
    } else if (false) {
        return get_confidence_interval_width(CONFIDENCE_ALPHA, error_variance, num_samples);
    } else {
        return get_prediction_interval_width(CONFIDENCE_ALPHA, error_variance, num_samples);
    }
}

void
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::observationAdded(double value)
{
    /*
     * Algorithm borrowed from
     * http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm
     * which cites Knuth's /The Art of Computer Programming/, Volume 1.
     */
    ++num_samples;
    double estimate = estimator->getEstimate();
    double error = calculate_error(estimate, value);
    error = log(error); // natural logarithm
    
    double delta = error - error_mean;
    error_mean += delta / num_samples;
    M2 += delta * (error - error_mean);
    if (num_samples > 1) {
        error_variance = M2 / (num_samples-1);
    }

    double bound_distance = getBoundDistance();
    double bounds[2];

    bounds[0] = exp(error_mean - bound_distance);
    bounds[1] = exp(error_mean + bound_distance);
    if (adjusted_estimate(estimate, bounds[0]) < adjusted_estimate(estimate, bounds[1])) {
        error_bounds[LOWER] = bounds[0];
        error_bounds[UPPER] = bounds[1];
    } else {
        error_bounds[LOWER] = bounds[1];
        error_bounds[UPPER] = bounds[0];
    }
    dbgprintf("Adding error sample to estimator %p: %f\n",
              estimator, exp(error));
    dbgprintf("n=%4d; error bounds: [%f, %f]\n", 
              num_samples, error_bounds[LOWER], error_bounds[UPPER]);
    if (adjusted_estimate(estimate, error_bounds[LOWER]) < 0.0) {
        // PROBLEMATIC.
        assert(false); // TODO: figure out what to do here.
    }
}


double
ConfidenceBoundsStrategyEvaluator::ErrorConfidenceBounds::getBound(BoundType type)
{
    double error = num_samples > 0 ? error_bounds[type] : no_error_value();
    return adjusted_estimate(estimator->getEstimate(), error);
}

void 
ConfidenceBoundsStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (bounds_by_estimator.count(estimator) == 0) {
        error_bounds.push_back(new ErrorConfidenceBounds(estimator));
        bounds_by_estimator[estimator] = error_bounds.back();
    } else {
        bounds_by_estimator[estimator]->observationAdded(value);
    }
}


ConfidenceBoundsStrategyEvaluator::EvalMode
 ConfidenceBoundsStrategyEvaluator::DEFAULT_EVAL_MODE = AGGRESSIVE;

ConfidenceBoundsStrategyEvaluator::ConfidenceBoundsStrategyEvaluator()
    : eval_mode(DEFAULT_EVAL_MODE), // TODO: set as option?
      step(0)
{
}

ConfidenceBoundsStrategyEvaluator::BoundType
ConfidenceBoundsStrategyEvaluator::getBoundType(EvalMode eval_mode, Strategy *strategy, 
                                                typesafe_eval_fn_t fn)
{
    if (eval_mode == AGGRESSIVE) {
        // aggressive = upper bound on benefit of redundancy.
        //            = max_time(singular) - min_time(redundant)
        // or:
        // aggressive = lower bound on additional cost of redundancy
        //            = min_cost(redundant) - max_cost(singular)
        // same bound for time and cost.
        return strategy->isRedundant() ? LOWER : UPPER;
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
                                                 void *strategy_arg, void *chooser_arg)
{
    BoundType bound_type = getBoundType(eval_mode, strategy, fn);
    return evaluateBounded(bound_type, fn, strategy_arg, chooser_arg);
}

typedef const double& (*bound_fn_t)(const double&, const double&);

double
ConfidenceBoundsStrategyEvaluator::evaluateBounded(BoundType bound_type, typesafe_eval_fn_t fn,
                                                   void *strategy_arg, void *chooser_arg)
{
    bound_fn_t bound_fns[UPPER + 1] = { &std::min<double>, &std::max<double> };
    bound_fn_t bound_fn = bound_fns[bound_type];

    // opposite fn, for calculating initial value that will
    //  always be replaced
    bound_fn_t bound_init_fn = bound_fns[(bound_type + 1) % (UPPER + 1)];

    // yes, it's a 2^N algorithm, but N is small; e.g. 4 for intnw
    unsigned int finish = 1 << error_bounds.size();
    double val = bound_init_fn(0.0, std::numeric_limits<double>::max());
    for (step = 0; step < finish; ++step) {
        val = bound_fn(val, fn(this, strategy_arg, chooser_arg));
    }
    return val;
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
            char bit = (step >> i) & 0x1;
            return est_error->getBound(BoundType(bit));
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

    // TODO
    out.close();
}

void 
ConfidenceBoundsStrategyEvaluator::restoreFromFile(const char *filename)
{
    ifstream in(filename);
    if (!in) {
        ostringstream oss;
        oss << "Failed to open " << filename;
        throw runtime_error(oss.str());
    }

    // TODO
    in.close();
}
