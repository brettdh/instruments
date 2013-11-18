#include <stdlib.h>
#include <math.h>

#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "instruments.h"
#include "instruments_private.h"

#include "test_common.h"
#include "empirical_error_strategy_evaluator_test.h"
#include "empirical_error_strategy_evaluator.h"
#include "eval_method.h"

#include "error_calculation.h"

#include "estimator.h"
#include "last_observation_estimator.h"

#include <sstream>
using std::ostringstream;

CPPUNIT_TEST_SUITE_REGISTRATION(EmpiricalErrorStrategyEvaluatorTest);

// should work either way.
//#define WIFI_SESSION_LENGTH_ESTIMATOR
#ifdef WIFI_SESSION_LENGTH_ESTIMATOR
const int NUM_INTNW_ESTIMATORS = 5;
const int CELLULAR_ESTIMATORS_INDEX = 3;
#else
const int NUM_INTNW_ESTIMATORS = 4;
const int CELLULAR_ESTIMATORS_INDEX = 2;
#endif



double get_time(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    Estimator *estimator = (Estimator *) strategy_arg;
    return get_adjusted_estimator_value(ctx, estimator);
}

double get_energy_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

double get_data_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

static const int NUM_ESTIMATORS = 5;

double get_time_all_estimators(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    Estimator **estimators = (Estimator**) strategy_arg;
    int num_estimators = (int) chooser_arg;
    double sum = 0.0;
    for (int i = 0; i < num_estimators; ++i) {
        sum += get_adjusted_estimator_value(ctx, estimators[i]);
    }
    return sum;
}

void 
EmpiricalErrorStrategyEvaluatorTest::testSimpleExpectedValue()
{
    Estimator *estimator = Estimator::create(LAST_OBSERVATION, "simple");
    Strategy *strategy = new Strategy(get_time, get_energy_cost, get_data_cost, estimator, NULL);
    strategy->addEstimator((typesafe_eval_fn_t) get_time, estimator);

    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *)&strategy, 1, 
                                                             EMPIRICAL_ERROR);
    // TODO: set it to the 'all samples' method

    for (int i = 9; i >= 4; --i) {
        estimator->addObservation((double) i);
    }

#ifdef RELATIVE_ERROR
    // empirical error distribution is (1.0, 8/9, 7/8, 6/7, 5/6, 4/5)
    // expected value is (4+32/9+7/2+24/7+10/3+16/5)/6 = 3.503
    double expected_value = 3.503;
#else
    // empirical error distribution is (0,1,1,1,1,1)
    // expected value is (4+3+3+3+3+3)/6 = 19/6
    double expected_value = 19.0/6;
#endif

    double value = evaluator->expectedValue(strategy, strategy->time_fn, strategy->strategy_arg, NULL);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_value, value, 0.001);
}

static void
create_estimators_and_strategies(Estimator **estimators, size_t num_estimators,
                                 Strategy **strategies, size_t num_strategies,
                                 EstimatorType estimator_type=RUNNING_MEAN)
{
    for (size_t i = 0; i < num_estimators; ++i) {
        ostringstream name;
        name << "estimator-" << i;
        estimators[i] = Estimator::create(estimator_type, name.str());
    }

    strategies[0] = new Strategy(get_time_all_estimators, 
                                 get_energy_cost, get_data_cost, estimators, (void *) CELLULAR_ESTIMATORS_INDEX);
    strategies[1] = new Strategy(get_time_all_estimators, 
                                 get_energy_cost, get_data_cost, 
                                 estimators + CELLULAR_ESTIMATORS_INDEX, (void *) 2);
    strategies[2] = new Strategy((instruments_strategy_t *) strategies, 2);
}

void
EmpiricalErrorStrategyEvaluatorTest::
assertRestoredEvaluationMatches(Strategy **strategies, double *expected_values,
                                size_t num_strategies, 
                                StrategyEvaluator *evaluator, void **chooser_args)
{
    for (size_t i = 0; i < num_strategies; ++i) {
        Strategy *strategy = strategies[i];
        double expected_value = expected_values[i];
        double value = evaluator->expectedValue(strategy, strategy->time_fn,
                                                strategy->strategy_arg, chooser_args[i]);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_value, value, 0.001);
    }
}

void 
EmpiricalErrorStrategyEvaluatorTest::testSaveRestore()
{
    const int NUM_STRATEGIES = 3;

    for (int n = 0; n < 5; ++n) {
        Estimator *estimators[NUM_INTNW_ESTIMATORS];
        Strategy *strategies[NUM_STRATEGIES];
        create_estimators_and_strategies(estimators, NUM_INTNW_ESTIMATORS,
                                         strategies, NUM_STRATEGIES);

        StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *) strategies, 
                                                                 NUM_STRATEGIES, 
                                                                 EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);

        for (int i = 0; i < NUM_INTNW_ESTIMATORS; ++i) {
            for (int j = 0; j < 20; ++j) {
                estimators[i]->addObservation(random());
            }
        }

        void *chooser_args[] = { (void *) CELLULAR_ESTIMATORS_INDEX, (void *) 2 };
        // skip redundant strategy, since its fn and arg are internal
        double expected_values[NUM_STRATEGIES-1];
        for (int i = 0; i < NUM_STRATEGIES-1; ++i) {
            Strategy *strategy = strategies[i];
            expected_values[i] = evaluator->expectedValue(strategy, strategy->time_fn,
                                                          strategy->strategy_arg, chooser_args[i]);
        }

        const char *FILENAME = "/tmp/instruments_saved_distribution.txt";
        evaluator->saveToFile(FILENAME);

        StrategyEvaluator *restored_evaluator = 
            StrategyEvaluator::create("", (instruments_strategy_t *) strategies, 
                                      NUM_STRATEGIES, 
                                      EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
        restored_evaluator->restoreFromFile(FILENAME);

        assertRestoredEvaluationMatches(strategies, expected_values, NUM_STRATEGIES - 1,
                                        evaluator, chooser_args);


        // now try it with newly-created estimators and strategies,
        //  as we'll do in IntNW
        Estimator *new_estimators[NUM_INTNW_ESTIMATORS];
        Strategy *new_strategies[NUM_STRATEGIES];
        create_estimators_and_strategies(new_estimators, NUM_INTNW_ESTIMATORS,
                                         new_strategies, NUM_STRATEGIES);
        StrategyEvaluator *new_evaluator = StrategyEvaluator::create("", (instruments_strategy_t *) new_strategies, 
                                                                     NUM_STRATEGIES, 
                                                                     EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
        new_evaluator->restoreFromFile(FILENAME);
        for (int i = 0; i < NUM_INTNW_ESTIMATORS; ++i) {
            new_estimators[i]->addObservation(estimators[i]->getEstimate());
        }
        
        assertRestoredEvaluationMatches(new_strategies, expected_values, NUM_STRATEGIES - 1,
                                        new_evaluator, chooser_args);
    }
}

void
EmpiricalErrorStrategyEvaluatorTest::testMultipleEstimators()
{
    Estimator *estimators[NUM_ESTIMATORS];
    for (int i = 0; i < NUM_ESTIMATORS; ++i) {
        ostringstream s;
        s << "estimator-" << i;
        estimators[i] = Estimator::create(LAST_OBSERVATION, s.str());
    }
    Strategy *strategy = new Strategy(get_time_all_estimators, 
                                      get_energy_cost, get_data_cost, estimators, (void *) NUM_ESTIMATORS);

    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *)&strategy, 1, 
                                                             EMPIRICAL_ERROR);
    // TODO: set it to the 'all samples' method

    for (int i = 0; i < NUM_ESTIMATORS; ++i) {
        double value = 5.0;
        double error = i;
        estimators[i]->addObservation(value);
        estimators[i]->addObservation(value - error*2);
    }

#ifdef RELATIVE_ERROR
    // estimate values: 
    //   5, 3, 1, -1, -3
    // relative error values:
    //   (1.0, 1.0), (0.6, 1.0), (0.2, 1.0), (-0.2, 1.0), (-0.6, 1.0)
    // adjusted error values:
    //   (5, 5), (1.8, 3), (0.2, 1), (0.2, -1.0), (1.8, -3)
    // sum of each permutation of those
    // sum of those, divided by 2^5.
    double expected_value = 7.0;
#else
    // values: sum of each permutation of:
    //   (5,5), (1,3), (-3,1), (-7,-1), (-11,-3)
    // sum of those, divided by 2^5.
    double expected_value = -5.0;
#endif

    double value = evaluator->expectedValue(strategy, strategy->time_fn, strategy->strategy_arg, (void *) NUM_ESTIMATORS);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_value, value, 0.001);
}

void
EmpiricalErrorStrategyEvaluatorTest::testMultipleEstimatorsTwice()
{
    testMultipleEstimators();
    testMultipleEstimators();
}

class CallCountEstimator : public LastObservationEstimator {
  public:
    CallCountEstimator() : LastObservationEstimator("call-count"), count(0) {}
    virtual double getEstimateLocked() {
        ++count;
        return LastObservationEstimator::getEstimateLocked();
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
    strategies[0] = new Strategy(get_time, get_energy_cost, get_data_cost,
                                 estimator1, NULL);
    CPPUNIT_ASSERT_EQUAL(1, estimator1->getCount());

    strategies[1] = new Strategy(get_time, get_energy_cost, get_data_cost,
                                 estimator2, NULL);
    CPPUNIT_ASSERT_EQUAL(1, estimator2->getCount());

    strategies[2] = new Strategy((instruments_strategy_t *) strategies, 2);
    CPPUNIT_ASSERT_EQUAL(2, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(2, estimator2->getCount());

    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *)strategies, 3,
                                                             EMPIRICAL_ERROR);
    estimator1->addObservation(0.0);
    estimator2->addObservation(0.0);
    // first observation adds no-error value to distribution;
    //  calls getEstimate to pass new estimate to subscribers.
    CPPUNIT_ASSERT_EQUAL(3, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(3, estimator2->getCount());

    // chooseStrategy should not call getEstimate
    //  when a strategy doesn't use the estimator.
    // Total new calls here: 2 per estimator
    (void)evaluator->chooseStrategy(NULL);
    int estimator1_count = estimator1->getCount();
    int estimator2_count = estimator2->getCount();
    CPPUNIT_ASSERT(estimator1_count >= 4);
    CPPUNIT_ASSERT(estimator1_count <= 5);
    CPPUNIT_ASSERT(estimator2_count >= 4);
    CPPUNIT_ASSERT(estimator2_count <= 5);

    estimator1->addObservation(0.0);
    estimator2->addObservation(0.0);
    // second observation adds error based on last value of estimator,
    //  so it does call getEstimate - once before and once after
    //  storing the new observation.
    CPPUNIT_ASSERT_EQUAL(estimator1_count + 2, estimator1->getCount());
    CPPUNIT_ASSERT_EQUAL(estimator2_count + 2, estimator2->getCount());
    estimator1_count = estimator1->getCount();
    estimator2_count = estimator2->getCount();

    // chooseStrategy should not call getEstimate
    //  when a strategy doesn't use the estimator.
    // Total new calls here:
    //  Naive case: 4, due to the joint-distribution iteration: 2x2
    //  Smarter case: the strategies are disjoint with respect to the
    //    estimators they use, so they can be computed in O(N) and cached
    //    for the 2x2 iteration.
    //   So, at least 2 new calls here.
    //  With simple iteration, it's at most 6.
    (void)evaluator->chooseStrategy(NULL);

    int estimator1_new_calls = (estimator1->getCount() - estimator1_count);
    int estimator2_new_calls = (estimator2->getCount() - estimator2_count);
    fprintf(stderr, "Estimator 1, new calls: %d\n", estimator1_new_calls);
    fprintf(stderr, "Estimator 2, new calls: %d\n", estimator2_new_calls);

    // lower now, due to memoization.
    CPPUNIT_ASSERT(estimator1_new_calls >= 2);
    CPPUNIT_ASSERT(estimator1_new_calls <= 6);
    CPPUNIT_ASSERT(estimator2_new_calls >= 2);
    CPPUNIT_ASSERT(estimator2_new_calls <= 6);
}

void 
EmpiricalErrorStrategyEvaluatorTest::testEstimatorConditions()
{
    //instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_DEBUG);

    const int NUM_STRATEGIES = 3;

    Estimator *estimators[NUM_INTNW_ESTIMATORS];
    Strategy *strategies[NUM_STRATEGIES];
    create_estimators_and_strategies(estimators, NUM_INTNW_ESTIMATORS,
                                     strategies, NUM_STRATEGIES,
                                     LAST_OBSERVATION);
    
    StrategyEvaluator *evaluator = StrategyEvaluator::create("", (instruments_strategy_t *) strategies, 
                                                             NUM_STRATEGIES, 
                                                             EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
    
    for (int i = 0; i < 11; ++i) {
        estimators[0]->addObservation((i % 2 == 0) ? 5.0 : 6.0);
        estimators[CELLULAR_ESTIMATORS_INDEX]->addObservation((i % 2 == 0) ? 1.0 : 20.0);
    }
    estimators[1]->addObservation(0.0);
#ifdef WIFI_SESSION_LENGTH_ESTIMATOR
    estimators[2]->addObservation(0.0);
#endif
    estimators[CELLULAR_ESTIMATORS_INDEX + 1]->addObservation(0.0);

    double mid_value = evaluator->expectedValue(strategies[0], strategies[0]->time_fn,
                                                strategies[0]->strategy_arg, (void *) CELLULAR_ESTIMATORS_INDEX);
    assertValueBetweenErrors("mid-value", mid_value, 
                             calculate_error(6.0, 5.0),
                             calculate_error(5.0, 6.0));

    double hilo_value = evaluator->expectedValue(strategies[1], strategies[1]->time_fn,
                                                 strategies[1]->strategy_arg, (void *) 2);
    assertValueBetweenErrors("hilo-value", hilo_value, 
                             calculate_error(20.0, 1.0),
                             calculate_error(1.0, 20.0));

    CPPUNIT_ASSERT_MESSAGE("mid-value wins", mid_value < hilo_value);

    estimators[CELLULAR_ESTIMATORS_INDEX]->setCondition(AT_MOST, 2.0);

    hilo_value = evaluator->expectedValue(strategies[1], strategies[1]->time_fn,
                                          strategies[1]->strategy_arg, (void *) 2);

    CPPUNIT_ASSERT_MESSAGE("hilo-conditional-value wins", hilo_value < mid_value);
    
#if 0
    // just kidding.  the weighting was bogus.  all such weights are now zero.
    // based on weighting of out-of-bounds samples
    double normalizer = 5.0/11.0 + (5.0/11.0 * 2.0 / 20.0) + 1.0/11.0;
    double expected = (1.0 * 1.0/11.0) + (1.0 * (1.0/20.0) * (5.0/11.0)) + (20.0 * (2.0/20.0) * (5.0/11.0));
#else
    // just removed the terms with zero-weights.
    double normalizer = 1.0/11.0 + 5.0/11.0;
    double expected = 1.0 * 1.0/11.0 + (1.0/20.0 * 5.0/11.0);
#endif
    MY_CPPUNIT_ASSERT_EQWITHIN_MESSAGE(expected/normalizer, hilo_value, 0.001, 
                                       "hilo-conditional-value matches expected");

    estimators[CELLULAR_ESTIMATORS_INDEX]->clearConditions();

    hilo_value = evaluator->expectedValue(strategies[1], strategies[1]->time_fn,
                                          strategies[1]->strategy_arg, (void *) 2);
    assertValueBetweenErrors("hilo-value", hilo_value, 
                             calculate_error(20.0, 1.0),
                             calculate_error(1.0, 20.0));

    CPPUNIT_ASSERT_MESSAGE("mid-value wins", mid_value < hilo_value);
}

void
EmpiricalErrorStrategyEvaluatorTest::assertValueBetweenErrors(const char *name, double value, 
                                                              double low_error, double high_error)
{
    char msg[256];
    double low_value = adjusted_estimate(value, low_error);
    double high_value = adjusted_estimate(value, high_error);

    snprintf(msg, 256, "%s (%f) is between %f and %f", 
             name, value, low_value, high_value);
    CPPUNIT_ASSERT_MESSAGE(msg, value >= low_value && value <= high_value);
}
