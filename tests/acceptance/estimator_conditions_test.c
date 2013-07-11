#include <instruments.h>
#include <eval_method.h>
#include <stdio.h>
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
    instruments_set_debug_level(INSTRUMENTS_DEBUG_LEVEL_DEBUG);

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

static double no_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

static void
check_chosen_strategy(instruments_strategy_evaluator_t evaluator,
                      instruments_strategy_t correct_strategy,
                      char *msg)
{
    instruments_strategy_t chosen = choose_nonredundant_strategy(evaluator, NULL);
    if (correct_strategy != chosen) {
        CTEST_LOG(msg);
    }
    ASSERT_EQUAL((int)correct_strategy, (int) chosen);
}

CTEST2(estimator_conditions, set_and_clear_condition)
{
    instruments_strategy_t strategies[3];
    int i;

    // use enough estimators to look like intnw
    struct test_data tdata[2] = {
        { &data->dummy[0], 2, data->mid_estimator },
        { &data->dummy[2], 1, data->hilo_estimator },
    };
    strategies[0] = make_strategy(estimator_value, NULL, no_cost, (void*) &tdata[0], NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, no_cost, (void*) &tdata[1], NULL);
    ASSERT_NOT_NULL(strategies[1]);
    strategies[2] = make_redundant_strategy(strategies, 2);
    ASSERT_NOT_NULL(strategies[2]);

    instruments_strategy_evaluator_t evaluator = register_strategy_set_with_method(strategies, 3,
                                                                                   EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);

    for (i = 0; i < 10; ++i) {
        double mid_value = (i % 2 == 0 ? 5.0 : 6.0);
        double hilo_value = (i % 2 == 0 ? 1.0 : 20.0);
        add_observation(data->mid_estimator, mid_value, mid_value);
        add_observation(data->hilo_estimator, hilo_value, hilo_value);
    }
    for (i = 0; i < 3; ++i) {
        add_observation(data->dummy[i], 0.0, 0.0);
    }
    
    check_chosen_strategy(evaluator, strategies[0], "Failed to choose mid strategy initially");

    set_estimator_condition(data->hilo_estimator, INSTRUMENTS_ESTIMATOR_VALUE_AT_MOST, 19.0);
    check_chosen_strategy(evaluator, strategies[1], "Failed to choose hilo strategy with not-high condition");

    clear_estimator_conditions(data->hilo_estimator);
    check_chosen_strategy(evaluator, strategies[0], "Failed to choose mid strategy after clearing conditions");
}
