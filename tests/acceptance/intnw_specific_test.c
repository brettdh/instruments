#include <instruments.h>
#include <instruments_private.h>
#include <resource_weights.h>

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <regex.h>

#include <ctest.h>

#include "debug.h"

struct strategy_args {
    int num_estimators;
    instruments_external_estimator_t *estimators;
    int is_cellular;
    double fake_cost; // hack to override cost for a particular test
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

    if (args->fake_cost >= 0.0) return args->fake_cost;

    // see below for reason for these costs.
    return (args->is_cellular ? 0.97 : 0.5);
}

#define NUM_ESTIMATORS 4
#define NUM_STRATEGIES 3

struct common_test_data {
    instruments_external_estimator_t estimators[NUM_ESTIMATORS];
    struct strategy_args args[NUM_STRATEGIES-1];
    instruments_strategy_t strategies[NUM_STRATEGIES];
    instruments_strategy_evaluator_t evaluator;
};

CTEST_DATA(intnw_specific_test) {
    struct common_test_data common_data;
};

static void 
create_estimators_and_strategies(instruments_external_estimator_t *estimators,
                                 instruments_strategy_t *strategies,
                                 struct strategy_args *args,
                                 instruments_strategy_evaluator_t *evaluator,
                                 enum EvalMethod method)
{
    int i;
    char name[64];
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        snprintf(name, 64, "estimator-%d", i);
        estimators[i] = create_external_estimator(name);
    }

    args[0].num_estimators = 2;
    args[0].estimators = &estimators[0];
    args[0].is_cellular = 0;
    args[0].fake_cost = -1.0;
    args[1].num_estimators = 2;
    args[1].estimators = &estimators[2];
    args[1].is_cellular = 1;
    args[1].fake_cost = -1.0;

    strategies[0] = make_strategy(network_time, NULL, data_cost, (void*) &args[0], NULL);
    strategies[1] = make_strategy(network_time, NULL, data_cost, (void*) &args[1], NULL);
    strategies[2] = make_redundant_strategy(strategies, 2);
    
    *evaluator = 
        register_strategy_set_with_method(strategies, NUM_STRATEGIES, method);
}

static void setup_common(struct common_test_data *cdata, enum EvalMethod method)
{
    set_fixed_resource_weights(0.0, 1.0);
    set_debug_level(NONE);

    cdata->evaluator = NULL;
    create_estimators_and_strategies(cdata->estimators, cdata->strategies, 
                                     cdata->args, &cdata->evaluator, method);
}

static void teardown_common(struct common_test_data *cdata)
{
    int i;
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        free_external_estimator(cdata->estimators[i]);
    }
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(cdata->strategies[i]);
    }
    free_strategy_evaluator(cdata->evaluator);
}

CTEST_SETUP(intnw_specific_test)
{
    setup_common(&data->common_data, EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
}

CTEST_TEARDOWN(intnw_specific_test)
{
    teardown_common(&data->common_data);
}

static void assert_correct_strategy_helper(struct common_test_data *cdata,
                                           instruments_strategy_t correct_strategy,
                                           int bytes, int soft)
{
    instruments_strategy_t *strategies = cdata->strategies;
    instruments_strategy_evaluator_t evaluator = cdata->evaluator;
    
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
    if (!soft) {
        ASSERT_EQUAL((int)correct_strategy, (int)chosen_strategy);
    }
}

static void assert_correct_strategy(struct common_test_data *cdata,
                                    instruments_strategy_t correct_strategy,
                                    int bytes)
{
    assert_correct_strategy_helper(cdata, correct_strategy, bytes, 0);
}

static void soft_assert_correct_strategy(struct common_test_data *cdata,
                                         instruments_strategy_t correct_strategy,
                                         int bytes)
{
    assert_correct_strategy_helper(cdata, correct_strategy, bytes, 1);
}


static void init_network_params(struct common_test_data *cdata,
                                double bandwidth1, double latency1, 
                                double bandwidth2, double latency2)
{
   
    int num_samples = 50;
    //int num_samples = 1;

    double values[] = { bandwidth1, latency1, bandwidth2, latency2 };

    int i, j;
    for (i = 0; i < 4; ++i) {
        // set single-bin histogram for the trivial bayesian tests.
        // i.e. if you're calling init_network_params, you're initializing
        // a network with very steady params for a sanity check.
        set_estimator_range_hints(cdata->estimators[i], values[i] * 0.9, values[i] * 1.1, 1);
    }

    for (i = 0; i < num_samples; ++i) {
        for (j = 0; j < 4; ++j) {
            add_observation(cdata->estimators[j], values[j], values[j]);
        }
    }
}

CTEST2(intnw_specific_test, test_one_network_wins)
{
    struct common_test_data *cdata = &data->common_data;

    int bytelen = 500000;
    init_network_params(cdata, 500000, 0.02, 128, 0.5);
    assert_correct_strategy(cdata, cdata->strategies[0], bytelen);
}

CTEST2(intnw_specific_test, test_each_network_wins)
{
    struct common_test_data *cdata = &data->common_data;
    
    init_network_params(cdata, 5000, 1.0, 2500, 0.2);
    // break-even: 
    //   x / 5000 + 1.0 = x / 2500 + 0.2
    //   x = 0.8 * 5000
    //   x = 4000
    //  if x > 4000, network 1 wins.
    //  if x < 4000, network 2 wins.
    
    assert_correct_strategy(cdata, cdata->strategies[0], 4001);
    assert_correct_strategy(cdata, cdata->strategies[1], 3999);
}



// params used for redundancy test
static int bytelen = 5000;
static double stable_bandwidth = 2000;
static double better_bandwidth = 5000;
static double worse_bandwidth = 1000;
// expected value of time on network 1: 2.5 seconds
//                   cost             : x
//                   time on network 2: bandwidth: 5000 +/- 4000
//                                      ((5/9)*25 + 5*25 + 1)/51 = 2.74
//                   cost             : y
//                   time on both-nets: ((5/9)*25 + 2.5*25 + 1)/51 = 1.51
//                   cost             : x + y
// net benefit: (2.5 - 1.52) - y
// so, need to set y < 0.98


static void add_last_estimates(instruments_external_estimator_t *estimators)
{
    add_observation(estimators[0], stable_bandwidth, stable_bandwidth);
    add_observation(estimators[1], 0.0, 0.0);
    add_observation(estimators[2], better_bandwidth, better_bandwidth);
    add_observation(estimators[3], 0.0, 0.0);
}

static double update_mean(double mean, double value, int n);

static void test_both_networks_best(struct common_test_data *cdata)
{
    int num_samples = 50;
    int i;
    
    //set_debug_level(DEBUG);

    set_estimator_range_hints(cdata->estimators[0], 500, 9500, 9);
    set_estimator_range_hints(cdata->estimators[1], -1, 1, 1);
    set_estimator_range_hints(cdata->estimators[2], 500, 9500, 9);
    set_estimator_range_hints(cdata->estimators[3], -1, 1, 1);

    // do this twice so that the bayesian method can make an initial decision.
    for (i = 0; i < 2; ++i) {
        add_observation(cdata->estimators[0], stable_bandwidth, stable_bandwidth);
        add_observation(cdata->estimators[1], 0.0, 0.0);
        add_observation(cdata->estimators[2], better_bandwidth, better_bandwidth);
        add_observation(cdata->estimators[3], 0.0, 0.0);
    }

    assert_correct_strategy(cdata, cdata->strategies[1], bytelen);
    
    double avg_bandwidth = 0.0;

    for (i = 0; i < num_samples; ++i) {
        add_observation(cdata->estimators[0], stable_bandwidth, stable_bandwidth);
        add_observation(cdata->estimators[1], 0.0, 0.0);

        double new_bandwidth;
        if (i % 2 == 0) {
            new_bandwidth = worse_bandwidth;
        } else {
            new_bandwidth = better_bandwidth;
        }
        avg_bandwidth = update_mean(avg_bandwidth, new_bandwidth, i);
        //double est_bandwidth = avg_bandwidth;
        double est_bandwidth = new_bandwidth;
        add_observation(cdata->estimators[2], new_bandwidth, est_bandwidth);

        add_observation(cdata->estimators[3], 0.0, 0.0);
    }

    assert_correct_strategy(cdata, cdata->strategies[2], bytelen);
}

CTEST2(intnw_specific_test, test_both_networks_best)
{
    test_both_networks_best(&data->common_data);
}

static void test_save_restore(struct common_test_data *cdata, const char *filename, 
                              enum EvalMethod eval_method)
{
    test_both_networks_best(cdata);
    save_evaluator(cdata->evaluator, filename);

    struct common_test_data new_data;
    create_estimators_and_strategies(new_data.estimators, new_data.strategies, 
                                     new_data.args, &new_data.evaluator,
                                     eval_method);
    add_last_estimates(new_data.estimators);
    if (eval_method == BAYESIAN) {
        // do it again, so that the bayesian method can make a decision.
        add_last_estimates(new_data.estimators);
    }

    // before restore, there's no error, so using the higher-bandwidth network is best
    assert_correct_strategy(&new_data, new_data.strategies[1], bytelen);

    restore_evaluator(new_data.evaluator, filename);
    assert_correct_strategy(&new_data, new_data.strategies[2], bytelen);
    add_last_estimates(new_data.estimators);
}

CTEST2(intnw_specific_test, test_save_restore)
{
    test_save_restore(&data->common_data, "/tmp/intnw_saved_evaluation_state.txt", 
                      EMPIRICAL_ERROR_ALL_SAMPLES_INTNW);
}

CTEST_DATA(confidence_bounds_test) {
    struct common_test_data common_data;
};

CTEST_SETUP(confidence_bounds_test)
{
    setup_common(&data->common_data, CONFIDENCE_BOUNDS);
}

CTEST_TEARDOWN(confidence_bounds_test)
{
    teardown_common(&data->common_data);
}

CTEST2(confidence_bounds_test, test_one_network_wins)
{
    struct common_test_data *cdata = &data->common_data;
    int bytelen = 500000;
    init_network_params(cdata, 500000, 0.02, 128, 0.5);
    assert_correct_strategy(cdata, cdata->strategies[0], bytelen);
}

CTEST2(confidence_bounds_test, test_each_network_wins)
{
    struct common_test_data *cdata = &data->common_data;
    init_network_params(cdata, 5000, 1.0, 2500, 0.2);
    // break-even: 
    //   x / 5000 + 1.0 = x / 2500 + 0.2
    //   x = 0.8 * 5000
    //   x = 4000
    //  if x > 4000, network 1 wins.
    //  if x < 4000, network 2 wins.
    
    assert_correct_strategy(cdata, cdata->strategies[0], 4001);
    assert_correct_strategy(cdata, cdata->strategies[1], 3999);
}

CTEST2(confidence_bounds_test, test_both_networks_best)
{
    test_both_networks_best(&data->common_data);
}

CTEST2(confidence_bounds_test, test_save_restore)
{
    test_save_restore(&data->common_data, "/tmp/confidence_bounds_saved_evaluation_state.txt", 
                      CONFIDENCE_BOUNDS);
    
}

static int
get_estimator_index(const char *network, const char *metric)
{
    static const char *networks[] = { "wifi", "3G" };
    static const char *metrics[] = { "bandwidth:", "latency:" };
    int network_id = -1;
    int metric_id = -1;
    int i;
    for (i = 0; i < 2; ++i) {
        if (!strcmp(network, networks[i])) {
            assert(network_id == -1);
            network_id = i;
        }
        if (!strcmp(metric, metrics[i])) {
            assert(metric_id == -1);
            metric_id = i;
        }
    }
    assert(network_id >= 0 && metric_id >= 0);
    return network_id * 2 + metric_id;
}


static void
run_real_distributions_test(struct common_test_data *cdata, const char *restore_filename)
{
    //set_debug_level(DEBUG);
    
    //const char *logfile = "./support_files/confidence_bounds_test_intnw.log";
    const char *logfile = "./support_files/post_restore_intnw.log";
    FILE *in = fopen(logfile, "r");
    assert(in);

    if (restore_filename) {
        restore_evaluator(cdata->evaluator, restore_filename);
    }

    char network[64], metric[64];
    double observation, estimate;
    char *line = NULL;
    size_t len = -1;
    
    // all 1 bits for the number of estimators
    size_t all_seen = (1 << NUM_ESTIMATORS) - 1;
    size_t seen = 0;
    // when all_seen == seen, I've seen a sample for all the estimators

    while ((getline(&line, &len, in)) != -1) {
        char *start = strstr(line, "Adding new stats to ");
        if (start) {
            int rc;
            int make_decision = (seen == all_seen);
            if ((rc = sscanf(start, "Adding new stats to %s network estimator: %s obs %lf est %lf",
                             network, metric, &observation, &estimate)) == 4) {
                int index = get_estimator_index(network, metric);
                add_observation(cdata->estimators[index], observation, estimate);
                seen |= (1 << index);
                
                if ((rc = sscanf(start, "Adding new stats to %*s network estimator: %*s obs %*f est %*f %s obs %lf est %lf",
                                 metric, &observation, &estimate)) == 3) {
                    index = get_estimator_index(network, metric);
                    add_observation(cdata->estimators[index], observation, estimate);
                    seen |= (1 << index);
                }
            }
            
            if (make_decision) {
                choose_strategy(cdata->evaluator, (void *) 1024);
            }
        }
        free(line);
        line = NULL;
    }
    fclose(in);
}

CTEST2(confidence_bounds_test, test_real_distributions)
{
    run_real_distributions_test(&data->common_data, 
                                "support_files/saved_error_distributions_prob.txt");
}

CTEST_DATA(bayesian_method_test) {
    struct common_test_data common_data;
};

CTEST_SETUP(bayesian_method_test)
{
    setup_common(&data->common_data, BAYESIAN);
    set_debug_level(DEBUG);
}

CTEST_TEARDOWN(bayesian_method_test)
{
    teardown_common(&data->common_data);
}

CTEST2(bayesian_method_test, test_one_network_wins)
{
    struct common_test_data *cdata = &data->common_data;
    int bytelen = 500000;
    init_network_params(cdata, 500000, 0.02, 128, 0.5);
    
    assert_correct_strategy(cdata, cdata->strategies[0], bytelen);
}

CTEST2(bayesian_method_test, test_each_network_wins)
{
    struct common_test_data *cdata = &data->common_data;
    init_network_params(cdata, 5000, 1.0, 2500, 0.2);
    // break-even: 
    //   x / 5000 + 1.0 = x / 2500 + 0.2
    //   x = 0.8 * 5000
    //   x = 4000
    //  if x > 4000, network 1 wins.
    //  if x < 4000, network 2 wins.
    
    assert_correct_strategy(cdata, cdata->strategies[0], 4001);
    assert_correct_strategy(cdata, cdata->strategies[1], 3999);
}

CTEST2(bayesian_method_test, test_both_networks_best)
{
    test_both_networks_best(&data->common_data);
}

CTEST2(bayesian_method_test, test_save_restore)
{
    test_save_restore(&data->common_data, "/tmp/bayesian_method_saved_evaluation_state.txt", 
                      BAYESIAN);
}

static double update_mean(double mean, double value, int n)
{
    return (mean * n + value) / (n + 1);
}

CTEST2(bayesian_method_test, test_proposal_example)
{
    //set_debug_level(DEBUG);
    
    struct common_test_data *cdata = &data->common_data;

    set_estimator_range_hints(cdata->estimators[0], 0.5, 9.5, 9);
    set_estimator_range_hints(cdata->estimators[1], -1, 1, 1);
    set_estimator_range_hints(cdata->estimators[2], 0.5, 9.5, 9);
    set_estimator_range_hints(cdata->estimators[3], -1, 1, 1);

    double bandwidth1_steps[] = { 9, 9, 8, 9, 1, 1, 1, 1, 1 };
    double bandwidth2_steps[] = { 5, 5, 4, 4, 5, 6, 5, 6, 6 };
    instruments_strategy_t correct_strategy[] = {
        cdata->strategies[0], // high-bandwidth network
        cdata->strategies[0],
        cdata->strategies[0],
        cdata->strategies[0],
        cdata->strategies[2], // high bandwidth dropped; now redundant
        cdata->strategies[2],
        cdata->strategies[2],
        cdata->strategies[2],
        cdata->strategies[1], // dropped-bandwidth network now consistently worse
    };

    double avg_bandwidth1 = 0.0;
    double avg_bandwidth2 = 0.0;
    const int num_steps = sizeof(bandwidth1_steps) / sizeof(double);
    int i;

    // no latency variation for this test.
    add_observation(cdata->estimators[1], 0.0, 0.0);
    add_observation(cdata->estimators[3], 0.0, 0.0);

    // make the two networks have the same small cost,
    // so that the instances in this example where the
    // redundant strategy is best still show that benefit > cost.
    // (in the example in my presentation, cost was zero.)
    cdata->args[0].fake_cost = 0.5;
    cdata->args[1].fake_cost = 0.5;

    //fprintf(stderr, "\n");
    for (i = 0; i < num_steps; ++i) {
        avg_bandwidth1 = update_mean(avg_bandwidth1, bandwidth1_steps[i], i);
        avg_bandwidth2 = update_mean(avg_bandwidth2, bandwidth2_steps[i], i);

        char msg[128];
        snprintf(msg, 127, "New bandwidths: N1 obs %f est %f  N2 obs %f est %f",
                 bandwidth1_steps[i], avg_bandwidth1,
                 bandwidth2_steps[i], avg_bandwidth2);
        CTEST_LOG(msg);
        //fprintf(stderr, "%s\n", msg);

        add_observation(cdata->estimators[0], bandwidth1_steps[i], avg_bandwidth1);
        add_observation(cdata->estimators[2], bandwidth2_steps[i], avg_bandwidth2);

        if (i > 0) {
            // skip the first one; I won't have a decision until
            // after the second estimator update.
            soft_assert_correct_strategy(cdata, correct_strategy[i], 10);
        }
    }
}

CTEST2(bayesian_method_test, test_real_distributions)
{
    struct common_test_data *cdata = &data->common_data;

    set_debug_level(DEBUG);
    run_real_distributions_test(cdata, NULL);
}

