#include "strategy_evaluation_context.h"
#include "estimator_context.h"

instruments_estimator_t 
StrategyEvaluationContext::getEstimatorContext(Estimator *estimator)
{
    if (estimators.count(estimator) == 0) {
        estimators[estimator] = new EstimatorContext(this, estimator);
    }
    return estimators[estimator];
}

StrategyEvaluationContext::~StrategyEvaluationContext()
{
    for (EstimatorMap::iterator it = estimators.begin(); 
         it != estimators.end(); ++it) {
        EstimatorContext *ctx = it->second;
        delete ctx;
    }
    estimators.clear();
}
