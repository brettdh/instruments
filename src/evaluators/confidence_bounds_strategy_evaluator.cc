#include "confidence_bounds_strategy_evaluator.h"
#include "estimator.h"

enum BoundType {
    LOWER=0, UPPER
};

class ErrorConfidenceBounds {
  public:
    ErrorConfidenceBounds(Estimator *estimator_);
    void observationAdded(double value);
    double getBound(BoundType type);

  private:
    static const double CONFIDENCE_ALPHA;

    Estimator *estimator;
    double error_mean;
    double error_variance;
    double M2; // for running variance algorithm
    size_t num_samples;
    
    double error_bounds[UPPER + 1];
};


const double ErrorConfidenceBounds::CONFIDENCE_ALPHA = 0.05;

ErrorConfidenceBounds::ErrorConfidenceBounds(Estimator *estimator_)
    : estimator(estimator_), error_mean(0.0), error_variance(0.0), M2(0.0), num_samples(0)
{
}

void
ErrorConfidenceBounds::observationAdded(double value)
{
    /*
     * Algorithm borrowed from
     * http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm
     * which cites Knuth's /The Art of Computer Programming/, Volume 1.
     */
    ++num_samples;
    double estimate = estimator->getEstimate();
    double error = calculate_error(estimate, value);
    double delta = error - error_mean;
    error_mean += delta / num_samples;
    M2 += delta * (error - error_mean);
    if (num_samples > 1) {
        error_variance = M2 / (n-1);
    }

    // Chebyshev interval for error bounds, as described in my proposal.
    double chebyshev_interval = sqrt(error_variance / CONFIDENCE_ALPHA);
    double bounds[2];
    bounds[0] = error_mean - chebyshev_interval;
    bounds[1] = error_mean + chebyshev_interval;
    if (adjusted_estimate(estimate, bounds[0]) < adjusted_estimate(estimate, bounds[1])) {
        error_bounds[LOWER] = bounds[0];
        error_bounds[UPPER] = bounds[1];
    } else {
        error_bounds[LOWER] = bounds[1];
        error_bounds[UPPER] = bounds[0];
    }
    if (adjusted_estimate(estimate, error_bounds[LOWER]) < 0.0) {
        // PROBLEMATIC.
        assert(false); // TODO: figure out what to do here.
    }
}


double
ErrorConfidenceBounds::getBound(BoundType type)
{
    assert(num_samples > 0); 
    // otherwise we would have just returned the estimate in getAdjustedEstimatorValue, below

    return adjusted_estimate(estimator->getEstimate(), error_bounds[type]);
}

void 
ConfidenceBoundsStrategyEvaluator::observationAdded(Estimator *estimator, double value)
{
    if (bounds_by_estimator.count(estimator) == 0) {
        bounds.push_back(new ErrorConfidenceBounds(estimator));
        bounds_by_estimator[estimator] = bounds.back();
    } else {
        bounds_by_estimator[estimator]->observationAdded(value);
    }
}


EvalMode ConfidenceBoundsStrategyEvaluator::DEFAULT_EVAL_MODE = AGGRESSIVE;

ConfidenceBoundsStrategyEvaluator::ConfidenceBoundsStrategyEvaluator()
    : eval_mode(DEFAULT_EVAL_MODE), // TODO: set as option?
      step(0)
{
}

BoundType
getBoundType(EvalMode eval_mode, Strategy *strategy, typesafe_eval_fn_t fn)
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
    return evaluateBounded(fn, strategy_arg, chooser_arg);
}

double
ConfidenceBoundsStrategyEvaluator::evaluateBounded(typesafe_eval_fn_t fn,
                                                   void *strategy_arg, void *chooser_arg)
{
    BoundType bound_type = getBoundType(eval_mode, strategy, fn);
    
    typedef double (*bound_fn_t)(double, double);
    bound_fn_t bound_fns[UPPER + 1] = { &std::min<double>, &std::max<double> };
    bound_fn_t bound_fn = bound_fns[bound_type];

    // opposite fn, for calculating initial value that will
    //  always be replaced
    bound_fn_t bound_init_fn = bound_fns[(bound_type + 1) % (UPPER + 1)];

    // yes, it's a 2^N algorithm, but N is small; e.g. 4 for intnw
    unsigned int finish = 1 << bounds.size();
    double val = bound_init_fn(0.0, std::numeric_limits<double>::max);
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
    for (size_t i = 0; i < bounds.size(); ++i) {
        if (est_error == bounds[i]) {
            char bit = (step >> i) & 0x1;
            return est_error->getBound(BoundType(bit));
        }
    }
    assert(false);
}
