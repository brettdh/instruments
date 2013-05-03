#include <instruments.h>
#include <instruments_private.h>

#include <stdio.h>
#include <assert.h>
#include <math.h>

#include "ctest.h"

CTEST_DATA(range_hints) {
    instruments_external_estimator_t high_estimator;
    instruments_external_estimator_t low_estimator;
};

CTEST_SETUP(range_hints)
{
    set_fixed_resource_weights(0.0, 1.0);

    data->high_estimator = create_external_estimator("high");
    data->low_estimator = create_external_estimator("low");
}

CTEST_TEARDOWN(range_hints)
{
    free_external_estimator(data->high_estimator);
    free_external_estimator(data->low_estimator);
}

static double
estimator_value(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    instruments_external_estimator_t estimator = 
        (instruments_external_estimator_t) strategy_arg;
    return get_estimator_value(ctx, estimator);
}

static double data_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 2.0;
}

static double normal_sample(double mean, double stddev)
{
    static int first = 1;
    static double prev_uniform = 0.0;

    if (first) {
        first = 0;
        prev_uniform = drand48();
    }
    double cur_uniform = drand48();
    double new_normal = sqrt(-2 * log(prev_uniform)) * sin(2 * M_PI * cur_uniform);
    prev_uniform = cur_uniform;
    return mean + (stddev * new_normal);
}

CTEST2(range_hints, value_observed)
{
    srand48(4242);
    double mean_low = 3.0;
    double mean_high = 10.0;
    double stddev = 0.5;

    const size_t NUM_BINS = 30;
    set_estimator_range_hints(data->low_estimator, 
                              mean_low - 3*stddev,
                              mean_low + 3*stddev,
                              NUM_BINS);
    set_estimator_range_hints(data->high_estimator, 
                              mean_high - 3*stddev,
                              mean_high + 3*stddev,
                              NUM_BINS);

    const int NUM_ESTIMATES = 100;
    int i;
    for (i = 0; i < NUM_ESTIMATES; ++i) {
        double low_sample = normal_sample(mean_low, stddev);
        add_observation(data->low_estimator, low_sample, low_sample);

        double high_sample = normal_sample(mean_high, stddev);
        add_observation(data->high_estimator, high_sample, high_sample);
    }

    instruments_strategy_t strategies[2];
    strategies[0] = make_strategy(estimator_value, NULL, data_cost, (void*) data->high_estimator, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, data_cost, (void*) data->low_estimator, NULL);
    ASSERT_NOT_NULL(strategies[1]);
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set_with_method(strategies, 2,
                                                                                   EMPIRICAL_ERROR_BINNED);
    instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[1], (int)chosen);
    // true if the external values were used.
}

