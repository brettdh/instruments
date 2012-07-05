#ifndef ESTIMATOR_CONTEXT_H_INCL
#define ESTIMATOR_CONTEXT_H_INCL


struct EstimatorContext {
    StrategyEvaluationContext *context;
    Estimator *estimator;

    EstimatorContext(StrategyEvaluationContext *c, Estimator *e)
        : context(c), estimator(e) {}
};

#endif
