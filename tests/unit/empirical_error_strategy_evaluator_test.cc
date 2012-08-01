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

double get_time(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    Estimator *estimator = (Estimator *) strategy_arg;
    return get_adjusted_estimator_value(ctx, estimator);
}

double get_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

static const int NUM_ESTIMATORS = 5;

double get_time_all_estimators(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    Estimator **estimators = (Estimator**) strategy_arg;
    double sum = 0.0;
    for (int i = 0; i < NUM_ESTIMATORS; ++i) {
        sum += get_adjusted_estimator_value(ctx, estimators[i]);
    }
    return sum;
}

void 
EmpiricalErrorStrategyEvaluatorTest::testSimpleExpectedValue()
{
    Estimator *estimator = Estimator::create(LAST_OBSERVATION);
    Strategy *strategy = new Strategy(get_time, get_cost, get_cost, estimator, NULL);
    strategy->addEstimator(estimator);

    StrategyEvaluator *evaluator = StrategyEvaluator::create((instruments_strategy_t *)&strategy, 1, 
                                                             EMPIRICAL_ERROR);
    // TODO: set it to the 'all samples' method

    for (int i = 9; i >= 4; --i) {
        estimator->addObservation((double) i);
    }
    // empirical error distribution is (0,1,1,1,1,1)
    // expected value is (4+3+3+3+3+3)/6 = 19/6

    double value = evaluator->expectedValue(strategy->time_fn, strategy->strategy_arg, NULL);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(19.0/6, value, 0.001);
}

void
EmpiricalErrorStrategyEvaluatorTest::testMultipleEstimators()
{
    Estimator *estimators[NUM_ESTIMATORS];
    for (int i = 0; i < NUM_ESTIMATORS; ++i) {
        estimators[i] = Estimator::create(LAST_OBSERVATION);
    }
    Strategy *strategy = new Strategy(get_time_all_estimators, 
                                      get_cost, get_cost, estimators, NULL);

    StrategyEvaluator *evaluator = StrategyEvaluator::create((instruments_strategy_t *)&strategy, 1, 
                                                             EMPIRICAL_ERROR);
    // TODO: set it to the 'all samples' method

    for (int i = 0; i < NUM_ESTIMATORS; ++i) {
        double value = 5.0;
        double error = i;
        estimators[i]->addObservation(value);
        estimators[i]->addObservation(value - error*2);
    }

    // values: sum of each permutation of:
    //   (5,5), (1,3), (-3,1), (-7,-1), (-11,-3)
    // sum of those, divided by 2^5.
    // expected value: -5.0

    double value = evaluator->expectedValue(strategy->time_fn, strategy->strategy_arg, NULL);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-5.0, value, 0.001);
}
