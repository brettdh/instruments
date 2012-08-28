#include <instruments.h>
#include <instruments_private.h>

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>

#include <ctest.h>

struct strategy_args {
    int num_estimators;
    instruments_external_estimator_t *estimators;
    int is_cellular;
};

static double
network_time(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    struct strategy_args *args = (struct strategy_args *) strategy_arg;
    int bytes = (int) chooser_arg;
    
    double bw = get_estimator_value(ctx, args->estimators[0]);
    double latency = get_estimator_value(ctx, args->estimators[1]);
    return bytes/bw + latency;
}

static double data_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    struct strategy_args *args = (struct strategy_args *) strategy_arg;

    // see below for reason for these costs.
    return (args->is_cellular ? 0.97 : 10.0);
}

#define NUM_ESTIMATORS 4
#define NUM_STRATEGIES 3

CTEST_DATA(intnw_specific_test) {
    instruments_external_estimator_t estimators[NUM_ESTIMATORS];
    struct strategy_args args[NUM_STRATEGIES-1];
    instruments_strategy_t strategies[NUM_STRATEGIES];
    instruments_strategy_evaluator_t evaluator;
};

CTEST_SETUP(intnw_specific_test)
{
    int i;
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        data->estimators[i] = create_external_estimator();
    }

    data->args[0].num_estimators = 2;
    data->args[0].estimators = &data->estimators[0];
    data->args[0].is_cellular = 0;
    data->args[1].num_estimators = 2;
    data->args[1].estimators = &data->estimators[2];
    data->args[1].is_cellular = 1;

    data->strategies[0] = make_strategy(network_time, NULL, data_cost, (void*) &data->args[0], NULL);
    data->strategies[1] = make_strategy(network_time, NULL, data_cost, (void*) &data->args[1], NULL);
    data->strategies[2] = make_redundant_strategy(data->strategies, 2);
    
    data->evaluator = 
        register_strategy_set_with_method(data->strategies, NUM_STRATEGIES,
                                          EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
}

CTEST_TEARDOWN(intnw_specific_test)
{
    int i;
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        free_external_estimator(data->estimators[i]);
    }
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(data->strategies[i]);
    }
    free_strategy_evaluator(data->evaluator);
}

static void assert_correct_strategy(struct intnw_specific_test_data *data, 
                                    instruments_strategy_t correct_strategy,
                                    int bytes)
{
    instruments_strategy_t *strategies = data->strategies;
    instruments_strategy_evaluator_t evaluator = data->evaluator;
    
    instruments_strategy_t chosen_strategy = choose_strategy(evaluator, (void *)bytes);
    int chosen_strategy_idx = -1, correct_strategy_idx = -1, i;
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        if (correct_strategy == strategies[i]) {
            correct_strategy_idx = i;
        }
        if (chosen_strategy == strategies[i]) {
            chosen_strategy_idx = i;
        }
    }
    ASSERT_NOT_EQUAL(-1, correct_strategy_idx);
    ASSERT_NOT_EQUAL(-1, chosen_strategy_idx);

    if (chosen_strategy != correct_strategy) {
        const char *names[] = {"'network 1''", "'network 2'", "'both networks'"};
        CTEST_LOG("Failed to choose %s; chose %s instead\n", 
                  names[correct_strategy_idx],
                  names[chosen_strategy_idx]);
    }
    ASSERT_EQUAL((int)correct_strategy, (int)chosen_strategy);
}

static void init_network_params(struct intnw_specific_test_data *data,
                                double bandwidth1, double latency1, 
                                double bandwidth2, double latency2)
{
    int num_samples = 50;
    //int num_samples = 1;

    int i;
    for (i = 0; i < num_samples; ++i) {
        add_observation(data->estimators[0], bandwidth1, bandwidth1);
        add_observation(data->estimators[1], latency1, latency1);
        add_observation(data->estimators[2], bandwidth2, bandwidth2);
        add_observation(data->estimators[3], latency2, latency2);
    }
}

CTEST2(intnw_specific_test, test_one_network_wins)
{
    int bytelen = 500000;
    init_network_params(data, 500000, 0.02, 128, 0.5);
    assert_correct_strategy(data, data->strategies[0], bytelen);
}

CTEST2(intnw_specific_test, test_each_network_wins)
{
    init_network_params(data, 5000, 1.0, 2500, 0.2);
    // break-even: 
    //   x / 5000 + 1.0 = x / 2500 + 0.2
    //   x = 0.8 * 5000
    //   x = 4000
    //  if x > 4000, network 1 wins.
    //  if x < 4000, network 2 wins.
    
    assert_correct_strategy(data, data->strategies[0], 4001);
    assert_correct_strategy(data, data->strategies[1], 3999);
}

CTEST2(intnw_specific_test, test_both_networks_best)
{
    int num_samples = 50;
    int i;
    int bytelen = 5000;
    double stable_bandwidth = 2000;
    double better_bandwidth = 5000;
    double worse_bandwidth = 1000;
    // expected value of time on network 1: 2.5 seconds
    //                   cost             : x
    //                   time on network 2: bandwidth: 5000 +/- 4000
    //                                      ((5/9)*25 + 5*25 + 1)/51 = 2.74
    //                   cost             : y
    //                   time on both-nets: ((5/9)*25 + 2.5*25 + 1)/51 = 1.51
    //                   cost             : x + y
    // net benefit: (2.5 - 1.52) - y
    // so, need to set y < 0.98
    
    add_observation(data->estimators[0], stable_bandwidth, stable_bandwidth);
    add_observation(data->estimators[1], 0.0, 0.0);
    add_observation(data->estimators[2], better_bandwidth, better_bandwidth);
    add_observation(data->estimators[3], 0.0, 0.0);
    
    for (i = 0; i < num_samples; ++i) {
        add_observation(data->estimators[0], stable_bandwidth, stable_bandwidth);
        add_observation(data->estimators[1], 0.0, 0.0);
        if (i % 2 == 0) {
            add_observation(data->estimators[2], worse_bandwidth, worse_bandwidth);
        } else {
            add_observation(data->estimators[2], better_bandwidth, better_bandwidth);
        }
        add_observation(data->estimators[3], 0.0, 0.0);
    }

    assert_correct_strategy(data, data->strategies[2], bytelen);
}
