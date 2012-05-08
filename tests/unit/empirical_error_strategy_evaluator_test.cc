#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "instruments.h"
#include "instruments_private.h"

#include "empirical_error_strategy_evaluator_test.h"
#include "empirical_error_strategy_evaluator.h"
#include "eval_method.h"

#include "estimator.h"

CPPUNIT_TEST_SUITE_REGISTRATION(EmpiricalErrorStrategyEvaluatorTest);

double get_time(instruments_context_t ctx, void *arg)
{
    Estimator *estimator = (Estimator *) arg;
    return get_adjusted_estimator_value(ctx, estimator);
}

double get_cost(instruments_context_t ctx, void *arg)
{
    return 0.0;
}

void 
EmpiricalErrorStrategyEvaluatorTest::testSimpleExpectedValue()
{
    Estimator *estimator = Estimator::create(LAST_OBSERVATION);
    Strategy *strategy = new Strategy(get_time, get_cost, get_cost, estimator);
    strategy->addEstimator(estimator);

    StrategyEvaluator *evaluator = StrategyEvaluator::create((instruments_strategy_t *)&strategy, 1, 
                                                             EMPIRICAL_ERROR);
    // TODO: set it to the 'all samples' method

    estimator->addObservation(9.0);
    estimator->addObservation(8.0);
    estimator->addObservation(7.0);
    estimator->addObservation(6.0);
    estimator->addObservation(5.0);
    estimator->addObservation(4.0); // empirical error is +1

    double value = evaluator->expectedValue(strategy->time_fn, strategy->fn_arg);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(3.0, value, 0.001);
}
