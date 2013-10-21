#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "instruments.h"
#include "instruments_private.h"

#include "strategy_estimators_discovery_test.h"
#include "estimator.h"
#include "strategy.h"
#include "strategy_evaluator.h"

#include <assert.h>

CPPUNIT_TEST_SUITE_REGISTRATION(StrategyEstimatorsDiscoveryTest);

static const size_t NUM_ESTIMATORS = 3;

double eval_fn_estimators_range(instruments_context_t ctx, void *arg,
                                size_t first, size_t last)
{
    assert (last < NUM_ESTIMATORS);

    double sum = 0.0;
    Estimator **estimators = (Estimator **) arg;
    for (size_t i = first; i <= last; ++i) {
        sum += get_adjusted_estimator_value(ctx, estimators[i]);
    }
    return sum;
}
double eval_fn_with_all_estimators(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return eval_fn_estimators_range(ctx, strategy_arg, 0, NUM_ESTIMATORS - 1);
}

double eval_fn_with_all_estimators_two_steps(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    static size_t step = 0;
    size_t first, last;
    if (step % 2 == 0) {
        first = 0;
        last = NUM_ESTIMATORS / 2;
    } else {
        first = NUM_ESTIMATORS / 2 + 1;
        last = NUM_ESTIMATORS - 1;
    }
    ++step;
    return eval_fn_estimators_range(ctx, strategy_arg, first, last);
}

double eval_fn_no_estimators(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

void StrategyEstimatorsDiscoveryTest::testEstimatorsDiscoveredAtRegistration()
{
    Estimator *estimators[NUM_ESTIMATORS];
    for (size_t i = 0; i < NUM_ESTIMATORS; ++i) {
        char name[64];
        snprintf(name, 64, "estimator-%d", i);
        estimators[i] = Estimator::create(LAST_OBSERVATION, name);
        estimators[i]->addObservation(1.0);
    }
    Strategy *strategy = new Strategy(eval_fn_with_all_estimators,
                                      NULL,
                                      eval_fn_no_estimators,
                                      estimators, NULL);

    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *)&strategy, 1,
                                                             TRUSTED_ORACLE);
    for (size_t i = 0; i < NUM_ESTIMATORS; ++i) {
        CPPUNIT_ASSERT(strategy->usesEstimator(estimators[i]));
        CPPUNIT_ASSERT(evaluator->usesEstimator(estimators[i]));
    }
}

void StrategyEstimatorsDiscoveryTest::testEstimatorsDiscoveredUponLaterUse()
{
    Estimator *estimators[NUM_ESTIMATORS];
    for (size_t i = 0; i < NUM_ESTIMATORS; ++i) {
        char name[64];
        snprintf(name, 64, "estimator-%d", i);
        estimators[i] = Estimator::create(LAST_OBSERVATION, name);
        estimators[i]->addObservation(1.0);
    }
    Strategy *strategy = new Strategy(eval_fn_with_all_estimators_two_steps,
                                      NULL,
                                      eval_fn_no_estimators,
                                      estimators, NULL);

    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *)&strategy, 1,
                                                             TRUSTED_ORACLE);
    char msg[64];
    for (size_t i = 0; i <= NUM_ESTIMATORS / 2; ++i) {
        snprintf(msg, 64, "strategy uses estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, strategy->usesEstimator(estimators[i]));
        snprintf(msg, 64, "evaluator uses estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, evaluator->usesEstimator(estimators[i]));
    }
    for (size_t i = NUM_ESTIMATORS / 2 + 1; i < NUM_ESTIMATORS; ++i) {
        snprintf(msg, 64, "strategy doesn't use estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, !strategy->usesEstimator(estimators[i]));
        snprintf(msg, 64, "evaluator doesn't use estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, !evaluator->usesEstimator(estimators[i]));
    }

    evaluator->chooseStrategy(NULL); // invokes the time function again
    // the remaining estimators should now be used by both
    for (size_t i = 0; i < NUM_ESTIMATORS; ++i) {
        snprintf(msg, 64, "later, strategy uses estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, strategy->usesEstimator(estimators[i]));
        snprintf(msg, 64, "later, evaluator uses estimator %zu", i);
        CPPUNIT_ASSERT_MESSAGE(msg, evaluator->usesEstimator(estimators[i]));
    }
}

