#include <instruments.h>
#include <instruments_private.h>
#include <eval_method.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>
#include "ctest.h"

CTEST_DATA(estimator_conditions) {
    instruments_external_estimator_t mid_estimator;
    instruments_external_estimator_t hilo_estimator;

    instruments_external_estimator_t dummy[3];
};

struct test_data {
    instruments_estimator_t *dummy;
    int num_dummy;
    instruments_estimator_t estimator;
};

CTEST_SETUP(estimator_conditions)
{
    //instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_DEBUG);

    data->mid_estimator = create_external_estimator("mid");
    data->hilo_estimator = create_external_estimator("hilo");
    
    int i;
    for (i = 0; i < 3; ++i) {
        char name[64];
        snprintf(name, 64, "dummy%d", i);
        data->dummy[i] = create_external_estimator(name);
    }
}

CTEST_TEARDOWN(estimator_conditions)
{
    free_external_estimator(data->mid_estimator);
    free_external_estimator(data->hilo_estimator);

    instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_NONE);
}

static double estimator_value(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    struct test_data *tdata = (struct test_data *) strategy_arg;
    int i;
    for (i = 0; i < tdata->num_dummy; ++i) {
        // use enough estimators to look like intnw
        (void) get_estimator_value(ctx, tdata->dummy[i]);
    }
    return get_estimator_value(ctx, tdata->estimator);
}

static double high_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 1.0;
}

static void check_strategy(instruments_strategy_t correct_strategy,
                           instruments_strategy_t chosen_strategy,
                           enum EvalMethod method, 
                           char *msg)
{
    if (correct_strategy != chosen_strategy) {
        char buf[4096];
        snprintf(buf, 4096, "[%s] %s (expected %s, chose %s)", 
                 get_method_name(method), msg, 
                 get_strategy_name(correct_strategy),
                 get_strategy_name(chosen_strategy));
        CTEST_LOG(buf);
    }
    ASSERT_EQUAL((int)correct_strategy, (int) chosen_strategy);
}

static void
check_chosen_strategy(instruments_strategy_evaluator_t evaluator,
                      enum EvalMethod method, 
                      instruments_strategy_t correct_strategy,
                      char *msg)
{
    instruments_strategy_t chosen = choose_nonredundant_strategy(evaluator, NULL);
    check_strategy(correct_strategy, chosen, method, msg);
}

static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static instruments_strategy_t strategy;

static void set_reeval_condition(void *arg)
{
    CTEST_DATA(estimator_conditions) *data = (CTEST_DATA(estimator_conditions) *) arg;
    set_estimator_condition(data->hilo_estimator, INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST, 2.0);
}

static void set_chosen_strategy(instruments_strategy_t chosen_strategy, void *arg)
{
    pthread_mutex_lock(&mutex);
    strategy = chosen_strategy;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mutex);
    
}

static double frandom_in(double low, double high)
{
    assert(high - low > 0.0001);
    return low + (random() * (high - low) / ((double) RAND_MAX));
}

static void run_set_and_clear_condition(CTEST_DATA(estimator_conditions) *data,
                                        enum EvalMethod method)
{
    instruments_strategy_t strategies[3];
    int i;

    // use enough estimators to look like intnw
    struct test_data tdata[2] = {
        { &data->dummy[0], 2, data->mid_estimator },
        { &data->dummy[2], 1, data->hilo_estimator },
    };
    strategies[0] = make_strategy(estimator_value, NULL, high_cost, (void*) &tdata[0], NULL);
    ASSERT_NOT_NULL(strategies[0]);
    set_strategy_name(strategies[0], "mid");
    strategies[1] = make_strategy(estimator_value, NULL, high_cost, (void*) &tdata[1], NULL);
    ASSERT_NOT_NULL(strategies[1]);
    set_strategy_name(strategies[1], "hilo");
    strategies[2] = make_redundant_strategy(strategies, 2);
    ASSERT_NOT_NULL(strategies[2]);
    set_strategy_name(strategies[2], "both");

    set_estimator_range_hints(data->mid_estimator, 0.5, 20.5, 20);
    set_estimator_range_hints(data->hilo_estimator, 0.5, 20.5, 20);
    for (i = 0; i < 3; ++i) {
        set_estimator_range_hints(data->dummy[i], 0.5, 20.5, 20);
    }

    instruments_strategy_evaluator_t evaluator = 
        register_strategy_set_with_method(strategies, 3, method);

    for (i = 0; i < 3; ++i) {
        add_observation(data->dummy[i], 1.0, 1.0);
    }
    srand(42424242);
    int num_iterations = 21;
    for (i = 0; i < num_iterations; ++i) {
        double mid_value = frandom_in(5.0, 6.0);
        double hilo_value = (i % 2 == 1) ? frandom_in(20.0, 21.0) : frandom_in(1.0, 2.0);
        //CTEST_LOG("Adding observations: mid %f hilo %f", mid_value, hilo_value);
        add_observation(data->mid_estimator, mid_value, mid_value);
        add_observation(data->hilo_estimator, hilo_value, hilo_value);
        /*
        if (method & BAYESIAN) {
            hilo_value = (i % 2 == 1) ? frandom_in(20.0, 21.0) : frandom_in(1.0, 2.0);
            add_observation(data->hilo_estimator, hilo_value, hilo_value);
        }
        */
    }

    // no redundancy
    set_fixed_resource_weights(1.0, 1.0);
    
    check_chosen_strategy(evaluator, method, strategies[0], "Failed to choose mid strategy initially");

    set_estimator_condition(data->hilo_estimator, INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST, 2.0);
    check_chosen_strategy(evaluator, method, strategies[1], "Failed to choose hilo strategy with not-high condition");

    clear_estimator_conditions(data->hilo_estimator);
    check_chosen_strategy(evaluator, method, strategies[0], "Failed to choose mid strategy after clearing conditions");

    instruments_scheduled_reevaluation_t eval = 
        schedule_reevaluation(evaluator, NULL, 
                              set_reeval_condition, data, 
                              set_chosen_strategy, data,
                              0.25);
    pthread_mutex_lock(&mutex);
    strategy = NULL;
    while (strategy == NULL) {
        pthread_cond_wait(&cv, &mutex);
    }
    check_strategy(strategies[1], strategy, method, "Failed to choose hilo strategy after re-evaluation");
    pthread_mutex_unlock(&mutex);

    free_scheduled_reevaluation(eval);
}

CTEST2(estimator_conditions, set_and_clear_condition)
{
    run_set_and_clear_condition(data, EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
}

CTEST2(estimator_conditions, set_and_clear_condition_prob_bounds)
{
    instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_DEBUG);
    run_set_and_clear_condition(data, CONFIDENCE_BOUNDS);
}

CTEST2(estimator_conditions, set_and_clear_condition_bayesian)
{
    instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_NONE);
    run_set_and_clear_condition(data, BAYESIAN);
}
