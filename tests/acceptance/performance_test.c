#include <instruments.h>
#include <instruments_private.h>

#include <stdio.h>
#include <sys/time.h>
#include <assert.h>
#include <stdlib.h>
#include <math.h>

#if (defined(ANDROID) && defined(PROFILING_BUILD))
#include "prof.h"
#endif

#include "debug.h"

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

static struct timeval
time_choose_strategy(instruments_strategy_evaluator_t evaluator, int bytelen)
{
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    (void) choose_strategy(evaluator, (void *) bytelen);
    gettimeofday(&end, NULL);
    TIMEDIFF(begin, end, diff);
    return diff;
}

struct strategy_args {
    int num_estimators;
    instruments_external_estimator_t *estimators;
};

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

static double get_unit_uniform_sample()
{
    return ((double) random()) / RAND_MAX;
}

static double last_sample = -1.0;

static void reset_prng()
{
    srandom(424242);
    last_sample = -1.0;
}


static double normal_mean = 5.0;
static double normal_stddev = 1.0;

// Box-Muller transform to get normally-distributed samples 
//  from uniformly-distributed samples.
static double get_sample()
{
    if (last_sample < 0.0) {
        last_sample = get_unit_uniform_sample();
    }
    double cur_sample = get_unit_uniform_sample();
    double normal_sample = sqrt(-2 * log(last_sample)) * cos(2 * M_PI * cur_sample);
    last_sample = cur_sample;

    // convert N(0, 1) to N(m, s)
    return normal_stddev * normal_sample + normal_mean;
}

static const size_t NUM_ESTIMATORS = 4;
static const size_t NUM_STRATEGIES = 3;

static void init_estimators(instruments_external_estimator_t *estimators,
                            int num_samples)
{
    int i;
    char name[64];
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        snprintf(name, 64, "estimator-%d", i);
        estimators[i] = create_external_estimator(name);

        // set bin hints for bayesian method -- scale with num_samples
        double range = normal_stddev * 2.0;
        set_estimator_range_hints(estimators[i], normal_mean - range, normal_mean + range, num_samples);
    }
}

static struct timeval run_test(int num_samples, enum EvalMethod method,
                               const char *restore_file)
{
    instruments_external_estimator_t estimators[NUM_ESTIMATORS];
    instruments_strategy_t strategies[NUM_STRATEGIES];
    init_estimators(estimators, num_samples);

    struct strategy_args args[2] = {
        {
        num_estimators: 2,
        estimators: &estimators[0]
        },
        {
        num_estimators: 2,
        estimators: &estimators[2]
        },
    };

    strategies[0] = make_strategy(estimator_value, NULL, no_cost, (void*) &args[0], NULL);
    strategies[1] = make_strategy(estimator_value, NULL, no_cost, (void*) &args[1], NULL);
    strategies[2] = make_redundant_strategy(strategies, 2);
    
    instruments_strategy_evaluator_t evaluator = 
        register_strategy_set_with_method(strategies, 3, method);

    if (restore_file) {
        restore_evaluator(evaluator, restore_file);
    }

    int bytelen = 4096;

    reset_prng();
    int i, j;
    for (i = 0; i < num_samples; ++i) {
        for (j = 0; j < NUM_ESTIMATORS; ++j) {
            add_observation(estimators[j], get_sample(), get_sample());
        }
    }
    struct timeval duration = time_choose_strategy(evaluator, bytelen);
    
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        free_external_estimator(estimators[i]);
    }

    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(strategies[i]);
    }
    free_strategy_evaluator(evaluator);
    
    return duration;
}


int main(int argc, char *argv[])
{
    int i;
    instruments_debug_level_t debug_level = NONE;
    if (argc > 1 && !strcasecmp(argv[1], "debug")) {
        debug_level = DEBUG;
    }
    instruments_set_debug_level(debug_level);

#if (defined(ANDROID) && defined(PROFILING_BUILD))
    monstartup("libinstruments.so");
#endif

    enum EvalMethod methods[] = { 
        //CONFIDENCE_BOUNDS,
        BAYESIAN,
        //EMPIRICAL_ERROR_ALL_SAMPLES,
        //EMPIRICAL_ERROR_ALL_SAMPLES_INTNW
    };
    const size_t NUM_METHODS = sizeof(methods) / sizeof(enum EvalMethod);
    
    fprintf(stderr, "%11s", "");
    for (i = 0; i < NUM_METHODS; ++i) {
        enum EvalMethod method = methods[i];
        fprintf(stderr, " %-15s", get_method_name(method));
    }
    fprintf(stderr, "\n");

    int min_samples = 5;
    int max_samples = 50;
    int new_samples = 5;
    int num_samples;

    for (num_samples = min_samples; num_samples <= max_samples; 
         num_samples += new_samples) {
        fprintf(stderr, "%3d samples", num_samples);
        for (i = 0; i < NUM_METHODS; ++i) {
            enum EvalMethod method = methods[i];
            struct timeval duration = run_test(num_samples, method, NULL);
            fprintf(stderr, " %lu.%06lu%7s", duration.tv_sec, duration.tv_usec, "");
        }
        fprintf(stderr, "\n");
    }

#ifdef ANDROID
    const char *bayesian_history = "/sdcard/intnw/instruments_test/saved_error_distributions_bayesian.txt";
#else
    const char *bayesian_history = "support_files/saved_error_distributions_bayesian.txt";
#endif
    fprintf(stderr, "%11s bayesian-with-history\n", "");
    for (num_samples = min_samples; num_samples <= max_samples;
         num_samples += new_samples) {
        struct timeval duration = run_test(num_samples, BAYESIAN, bayesian_history);
        fprintf(stderr, "%3d samples %lu.%06lu\n", num_samples, duration.tv_sec, duration.tv_usec);
    }
    
//#define CHECK_VALUES
#ifdef CHECK_VALUES
    instruments_set_debug_level(DEBUG);
    for (i = 0; i < NUM_METHODS; ++i) {
        enum EvalMethod method = methods[i];
        fprintf(stderr, "*** %s ***\n", get_method_name(method));
        (void) run_test(5, method, NULL);
    }
#endif

#if (defined(ANDROID) && defined(PROFILING_BUILD))
    moncleanup();
#endif
    return 0;
}
