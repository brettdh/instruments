#include <instruments.h>
#include <instruments_private.h>

#include "ctest.h"

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>

#define TIMEDIFF(tvb,tve,tvr)                                    \
do {                                                             \
    assert(((tve).tv_sec > (tvb).tv_sec)                         \
           || (((tve).tv_sec == (tvb).tv_sec)                    \
               && (tve.tv_usec >= tvb.tv_usec)));             \
    if (tve.tv_usec < tvb.tv_usec) {                         \
        tvr.tv_usec = 1000000 + tve.tv_usec - tvb.tv_usec; \
        (tvr).tv_sec = (tve).tv_sec - (tvb).tv_sec - 1;          \
    } else {                                                     \
        tvr.tv_usec = tve.tv_usec - tvb.tv_usec;           \
        (tvr).tv_sec = (tve).tv_sec - (tvb).tv_sec;              \
    }                                                            \
} while (0)

#define TIME_FN_CALL(TITLE, FUNCCALL)                   \
    do {                                                \
        struct timeval begin, end, diff;                \
        gettimeofday(&begin, NULL);                     \
        FUNCCALL;                                       \
        gettimeofday(&end, NULL);                       \
        TIMEDIFF(begin, end, diff);                     \
        fprintf(stderr, "%20s: %lu.%06lu seconds\n",    \
                TITLE, diff.tv_sec, diff.tv_usec);      \
    } while (0)



#define NUM_ESTIMATORS 4

CTEST_DATA(brute_force_perf) {
    instruments_external_estimator_t estimators[NUM_ESTIMATORS];
};

struct strategy_args {
    int num_estimators;
    instruments_external_estimator_t *estimators;
};

CTEST_SETUP(brute_force_perf)
{
    int i;
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        data->estimators[i] = create_external_estimator();
    }
}

CTEST_TEARDOWN(brute_force_perf)
{
    int i;
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        free_external_estimator(data->estimators[i]);
    }
}

static double
estimator_value(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    struct strategy_args *args = (struct strategy_args *) strategy_arg;
    int bytes = (int) chooser_arg;
    
    double bw = get_estimator_value(ctx, args->estimators[0]);
    double latency = get_estimator_value(ctx, args->estimators[1]);
    return bytes/bw + latency;
}

static double no_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}


CTEST2(brute_force_perf, value_observed)
{
    struct strategy_args args[2] = {
        {
        num_estimators: 2,
        estimators: &data->estimators[0]
        },
        {
        num_estimators: 2,
        estimators: &data->estimators[2]
        },
    };

    instruments_strategy_t strategies[3];
    strategies[0] = make_strategy(estimator_value, NULL, no_cost, (void*) &args[0], NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(estimator_value, NULL, no_cost, (void*) &args[1], NULL);
    ASSERT_NOT_NULL(strategies[1]);
    strategies[2] = make_redundant_strategy(strategies, 2);
    ASSERT_NOT_NULL(strategies[2]);
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set_with_method(strategies, 3,
                                                                                   EMPIRICAL_ERROR);

    int bytelen = 4096;
    //int max_samples = 30;
    int max_samples = 500;
    int num_new_samples = 5;
    int k;

    for (k = 0; k < max_samples / num_new_samples; ++k) {
        int i, j;
        char title[64];
        for (i = 0; i < num_new_samples; ++i) {
            for (j = 0; j < NUM_ESTIMATORS; ++j) {
                add_observation(data->estimators[j], random(), random());
            }
        }
        
        snprintf(title, 64, "choose_strategy, %d samples", num_new_samples * (k+1));
        TIME_FN_CALL(title, choose_strategy(evaluator, (void*) bytelen));
    }
}
