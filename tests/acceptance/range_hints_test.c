#include <instruments.h>
#include <instruments_private.h>
#include "debug.h"

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
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set_with_method("", strategies, 2,
                                                                                   EMPIRICAL_ERROR_BINNED);
    instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[1], (int)chosen);
    // true if the external values were used.
}


CTEST2(range_hints, tail_values_are_okay)
{
    instruments_set_debug_level(NONE);
    
    set_estimator_range_hints(data->low_estimator, 1, 9, 8);
    set_estimator_range_hints(data->high_estimator, 11, 19, 8);

    instruments_strategy_t strategies[3];
    strategies[0] = make_strategy(estimator_value, NULL, data_cost, (void*) data->high_estimator, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, data_cost, (void*) data->low_estimator, NULL);
    ASSERT_NOT_NULL(strategies[1]);
    strategies[2] = make_redundant_strategy(strategies, 2);
    ASSERT_NOT_NULL(strategies[2]);
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set_with_method("", strategies, 3, BAYESIAN);
    
    int i;
    for (i = 1; i < 9; ++i) {
        // add a bunch of non-tail observations
        add_observation(data->low_estimator, 0.5 + i, 0.5 + i);
        add_observation(data->high_estimator, 10.5 + i, 10.5 + i);
    }

    int n;
    for (n = 0; n < 10; ++n) {
        for (i = 1; i < 9; ++i) {
            // add a bunch of non-tail observations
            add_observation(data->low_estimator, 0.5 + i, 0.5 + i);
            add_observation(data->high_estimator, 10.5 + i, 10.5 + i);
        }

        // failure case is described below.  No tail values yet, so 
        //  I shouldn't see the failure yet.
        instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
        ASSERT_NOT_NULL(chosen);
    }

    for (n = 0; n < 50; ++n) {
        for (i = 0; i < 5; ++i) {
            // add *different* observations that fall in the tail.
            // checking whether the binned distribution handles this correctly.
            // if not, the bug will show up as a PDF summing to greater than 1
            // due to double-counting.  this will trip an assertion in libinstruments.
            
            double low_obs, high_obs;
            if (i % 2 == 0) {
                low_obs = 0.0;
                high_obs = 19.0;
            } else {
                low_obs = 9.0;
                high_obs = 10.0;
            }
            // make the observations different, but less than the tail break.
            low_obs += i / 5.0;
            high_obs += i / 5.0;
            add_observation(data->low_estimator, low_obs, low_obs);
            add_observation(data->high_estimator, high_obs, high_obs);
        }
        
        // just in case it hasn't failed yet, force a weighted sum.
        instruments_strategy_t chosen = choose_strategy(evaluator, NULL);
        ASSERT_NOT_NULL(chosen);
    }
}
