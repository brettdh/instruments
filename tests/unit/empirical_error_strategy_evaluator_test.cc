#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "instruments.h"
#include "instruments_private.h"

#include "empirical_error_strategy_evaluator_test.h"
#include "empirical_error_strategy_evaluator.h"
#include "eval_method.h"

#include "estimator.h"
#include "last_observation_estimator.h"

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

    double value = evaluator->expectedValue(strategy, strategy->time_fn, strategy->strategy_arg, NULL);
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

    double value = evaluator->expectedValue(strategy, strategy->time_fn, strategy->strategy_arg, NULL);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(-5.0, value, 0.001);
}

void
EmpiricalErrorStrategyEvaluatorTest::testMultipleEstimatorsTwice()
{
    testMultipleEstimators();
    testMultipleEstimators();
}

class CallCountEstimator : public LastObservationEstimator {
  public:
    CallCountEstimator() : count(0) {}
    virtual double getEstimate() {
        ++count;
        return LastObservationEstimator::getEstimate();
    }
    int getCount() { return count; }
  private:
    int count;
};

void
EmpiricalErrorStrategyEvaluatorTest::testOnlyIterateOverRelevantEstimators()
{
    CallCountEstimator *estimator1 = new CallCountEstimator;
    CallCountEstimator *estimator2 = new CallCountEstimator;

    Strategy *strategies[3];
    strategies[0] = new Strategy(get_time, get_cost, get_cost,
                                 estimator1, NULL);
    CPPUNIT_ASSERT_EQUAL(1, estimator1->getCount());

    strategies[1] = new Strategy(get_time, get_cost, get_cost,
                                 estimator2, NULL);
    CPPUNIT_ASSERT_EQUAL(1, estimator2->getCount());

    strategies[2] = new Strategy((instruments_strategy_t *) strategies, 2);
    CPPUNIT_ASSERT_EQUAL(2, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(2, estimator2->getCount());

    StrategyEvaluator *evaluator = StrategyEvaluator::create((instruments_strategy_t *)strategies, 3,
                                                             EMPIRICAL_ERROR);
    estimator1->addObservation(0.0);
    estimator2->addObservation(0.0);
    // first observation adds error of zero to distribution;
    //  doesn't call getEstimate.
    CPPUNIT_ASSERT_EQUAL(2, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(2, estimator2->getCount());

    // chooseStrategy should not call getEstimate
    //  when a strategy doesn't use the estimator.
    // Total new calls here: 2 per estimator
    (void)evaluator->chooseStrategy(NULL);
    int estimator1_count = estimator1->getCount();
    int estimator2_count = estimator2->getCount();
    CPPUNIT_ASSERT(estimator1_count >= 3);
    CPPUNIT_ASSERT(estimator1_count <= 4);
    CPPUNIT_ASSERT(estimator2_count >= 3);
    CPPUNIT_ASSERT(estimator2_count <= 4);

    estimator1->addObservation(0.0);
    estimator2->addObservation(0.0);
    // second observation adds error based on last value of estimator,
    //  so it does call getEstimate.
    CPPUNIT_ASSERT_EQUAL(estimator1_count + 1, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(estimator2_count + 1, estimator2->getCount());
    estimator1_count = estimator1->getCount();
    estimator2_count = estimator2->getCount();

    // chooseStrategy should not call getEstimate
    //  when a strategy doesn't use the estimator.
    // Total new calls here:
    //  At least 4, due to the joint-distribution iteration: 2x2
    //  With simple iteration, it's at most 6.
    //  With some caching, though, it could be just 4, maybe.
    (void)evaluator->chooseStrategy(NULL);

    int estimator1_new_calls = (estimator1->getCount() - estimator1_count);
    int estimator2_new_calls = (estimator2->getCount() - estimator2_count);
    fprintf(stderr, "Estimator 1, new calls: %d\n", estimator1_new_calls);
    fprintf(stderr, "Estimator 2, new calls: %d\n", estimator2_new_calls);

    // lower now, due to memoization.
    CPPUNIT_ASSERT(estimator1_new_calls >= 4);
    CPPUNIT_ASSERT(estimator1_new_calls <= 6);
    CPPUNIT_ASSERT(estimator2_new_calls >= 4);
    CPPUNIT_ASSERT(estimator2_new_calls <= 6);
}
