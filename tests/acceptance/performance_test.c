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
time_choose_strategy(instruments_strategy_evaluator_t evaluator, int bytelen, int redundant)
{
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    if (redundant) {
        (void) choose_strategy(evaluator, (void *) bytelen);
    } else {
        (void) choose_nonredundant_strategy(evaluator, (void *) bytelen);
    }
    gettimeofday(&end, NULL);
    TIMEDIFF(begin, end, diff);
    return diff;
}

struct strategy_args {
    int num_estimators;
    instruments_external_estimator_t *estimators;
    instruments_external_estimator_t *other_estimators;
};

static double
estimator_value(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    struct strategy_args *args = (struct strategy_args *) strategy_arg;
    int bytes = (int) chooser_arg;
    
    double bw = get_estimator_value(ctx, args->estimators[0]);
    double latency = get_estimator_value(ctx, args->estimators[1]);
    if (args->num_estimators > 2) {
        get_estimator_value(ctx, args->estimators[2]);
    }
    if (args->other_estimators) {
        bw += get_estimator_value(NULL, args->other_estimators[0]);
        latency += get_estimator_value(NULL, args->other_estimators[1]);
    }
    return bytes/bw + latency;
}

static double no_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return estimator_value(ctx, strategy_arg, chooser_arg) * 2.0;
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

// should work either way.
//#define WIFI_SESSION_LENGTH_ESTIMATOR
#ifdef WIFI_SESSION_LENGTH_ESTIMATOR
#define NUM_WIFI_ESTIMATORS 3
#else
#define NUM_WIFI_ESTIMATORS 2
#endif

static const int CELLULAR_ESTIMATORS_INDEX = NUM_WIFI_ESTIMATORS;
#define NUM_CELLULAR_ESTIMATORS 2
static const int NUM_ESTIMATORS = NUM_WIFI_ESTIMATORS + NUM_CELLULAR_ESTIMATORS;

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
                               const char *restore_file, int choose_strategy_count,
                               int redundant)
{
    instruments_external_estimator_t estimators[NUM_ESTIMATORS];
    instruments_strategy_t strategies[NUM_STRATEGIES];
    init_estimators(estimators, num_samples);

    struct strategy_args args[2] = {
        {
        num_estimators: NUM_WIFI_ESTIMATORS,
        estimators: &estimators[0],
        other_estimators: &estimators[CELLULAR_ESTIMATORS_INDEX]
        },
        {
        num_estimators: NUM_CELLULAR_ESTIMATORS,
        estimators: &estimators[CELLULAR_ESTIMATORS_INDEX],
        other_estimators: NULL
        },
    };

    strategies[0] = make_strategy(estimator_value, no_cost, no_cost, (void*) &args[0], NULL);
    strategies[1] = make_strategy(estimator_value, no_cost, no_cost, (void*) &args[1], NULL);
    strategies[2] = make_redundant_strategy(strategies, 2, NULL);
    
    instruments_strategy_evaluator_t evaluator = 
        register_strategy_set_with_method("", strategies, 3, method);

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
    struct timeval total_duration = {0, 0};
    for (i = 0; i < choose_strategy_count; ++i) {
        struct timeval duration = time_choose_strategy(evaluator, bytelen, redundant);
        timeradd(&total_duration, &duration, &total_duration);
        
        add_observation(estimators[i % NUM_ESTIMATORS], get_sample(), get_sample());
    }
    
    for (i = 0; i < NUM_ESTIMATORS; ++i) {
        free_external_estimator(estimators[i]);
    }

    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(strategies[i]);
    }
    free_strategy_evaluator(evaluator);
    
    return total_duration;
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
        //BAYESIAN,
        EMPIRICAL_ERROR_ALL_SAMPLES,
        //EMPIRICAL_ERROR_ALL_SAMPLES_INTNW,
        //EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED,
        //EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED_INTNW
    };
    const size_t NUM_METHODS = sizeof(methods) / sizeof(enum EvalMethod);
    
    int min_samples = 5;
    int max_samples = 50;
    int new_samples = 5;
    int num_samples;
    int redundant;

    for (redundant = 1; redundant >= 0; --redundant) {
        fprintf(stderr, "%sconsidering redundant strategy\n", 
                (redundant ? "" : "not "));
        fprintf(stderr, "%11s", "");
        for (i = 0; i < NUM_METHODS; ++i) {
            enum EvalMethod method = methods[i];
            fprintf(stderr, " %-15s", get_method_name(method));
        }
        fprintf(stderr, "\n");
        
        for (num_samples = min_samples; num_samples <= max_samples; 
             num_samples += new_samples) {
            fprintf(stderr, "%3d samples", num_samples);
            for (i = 0; i < NUM_METHODS; ++i) {
                enum EvalMethod method = methods[i];
                struct timeval duration = run_test(num_samples, method, NULL, 1, redundant);
                fprintf(stderr, " %lu.%06lu%7s", duration.tv_sec, duration.tv_usec, "");
            }
            fprintf(stderr, "\n");
        }
    }

#ifdef ANDROID
    const char *bayesian_history = "/sdcard/intnw/instruments_test/saved_error_distributions_bayesian.txt";
#else
    const char *bayesian_history = "support_files/saved_error_distributions_bayesian.txt";
#endif
    fprintf(stderr, "%11s bayesian-with-history\n", "");
    for (num_samples = min_samples; num_samples <= max_samples;
         num_samples += new_samples) {
        struct timeval duration = run_test(num_samples, BAYESIAN, bayesian_history, 1, 1);
        fprintf(stderr, "%3d samples %lu.%06lu\n", num_samples, duration.tv_sec, duration.tv_usec);
    }

#if 0
    int num_iterations = 1000;
    num_samples = 50;
    struct timeval total_duration = run_test(num_samples, CONFIDENCE_BOUNDS, NULL, num_iterations, 1);
    fprintf(stderr, "Confidence bounds, %d samples, %d times:  %lu.%06lu sec\n", 
            num_samples, num_iterations, total_duration.tv_sec, total_duration.tv_usec);
#endif    
    
//#define CHECK_VALUES
#ifdef CHECK_VALUES
    instruments_set_debug_level(DEBUG);
    for (i = 0; i < NUM_METHODS; ++i) {
        enum EvalMethod method = methods[i];
        fprintf(stderr, "*** %s ***\n", get_method_name(method));
        (void) run_test(5, method, NULL, 1, 1);
    }
#endif

#if (defined(ANDROID) && defined(PROFILING_BUILD))
    moncleanup();
#endif
    return 0;
}
