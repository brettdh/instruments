#ifndef EXPECTATION_CONTEXT_H_INCL
#define EXPECTATION_CONTEXT_H_INCL


/* expected inheritance:
 *   ExpectationContext
 *   |
 *   + IterationContext
 *   | |
 *   | + BruteForceContext
 *   | + BayesianContext
 *   |
 *   + ConfidenceBoundContext
 *
 * These help implement the different uncertainty evaluation methods
 *   by deciding how to return estimator values.
 *
 * XXX: these sound like they should really be part of the EstimatorGroup class.
 */
class ExpectationContext {
  public:
    double evaluate(eval_fn_t fn) = 0;
};

#endif
