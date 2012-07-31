#include <instruments.h>
#include <instruments_private.h>

#include "ctest.h"

CTEST_DATA(external_estimator) {
    instruments_external_estimator_t high_estimator;
    instruments_external_estimator_t low_estimator;
};

CTEST_SETUP(external_estimator)
{
    data->high_estimator = create_external_estimator();
    data->low_estimator = create_external_estimator();
}

CTEST_TEARDOWN(external_estimator)
{
    free_external_estimator(data->high_estimator);
    free_external_estimator(data->low_estimator);
}

static double
estimator_value(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    instruments_external_estimator_t estimator = 
        (instruments_external_estimator_t) strategy_arg;
    return get_external_estimator_value(ctx, estimator);
}

static double data_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 4.0;
}


CTEST2(external_estimator, value_observed)
{
    add_observation(data->high_estimator, 1.0, 1.0);
    add_observation(data->low_estimator, 0.0, 0.0);

    instruments_strategy_t strategies[2];
    strategies[0] = make_strategy(estimator_value, NULL, data_cost, (void*) data->high_estimator, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, data_cost, (void*) data->low_estimator, NULL);
    ASSERT_NOT_NULL(strategies[1]);
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set(strategies, 2);
    instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[1], (int)chosen);
    // true if the external values were used.
}

static void 
run_test_with_oscillating_estimator(struct external_estimator_data *data,
                                    double value1, double value2,
                                    int correct_strategy)
{
    instruments_strategy_t strategies[2];
    strategies[0] = make_strategy(estimator_value, NULL, data_cost, (void*) data->high_estimator, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, data_cost, (void*) data->low_estimator, NULL);
    ASSERT_NOT_NULL(strategies[1]);
    strategies[2] = make_redundant_strategy(strategies, 2);
    ASSERT_NOT_NULL(strategies[2]);
    
    instruments_strategy_evaluator_t evaluator = 
        register_strategy_set_with_method(strategies, 3, EMPIRICAL_ERROR);

    int i;
    for (i = 0; i < 25; ++i) {
        add_observation(data->high_estimator, value1, value2);
        add_observation(data->high_estimator, value2, value1);

        add_observation(data->low_estimator, 4.0, 5.0);
        add_observation(data->low_estimator, 5.0, 4.0);
    }

    instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[correct_strategy], (int)chosen);
    // true if the error from the external strategies was used.
}

CTEST2(external_estimator, singular_strategy_chosen)
{
    run_test_with_oscillating_estimator(data, 10.0, 9.0, 1);
}

CTEST2(external_estimator, redundant_strategy_chosen)
{
    run_test_with_oscillating_estimator(data, 10.0, 0.0, 2);
}
