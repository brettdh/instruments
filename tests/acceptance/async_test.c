#include <instruments.h>
#include "ctest.h"

#include <pthread.h>
#include "pthread_util.h"

static double
one_of_two_values(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    double times[2] = {5.0, 10.0};
    int index = (int) strategy_arg + (int) chooser_arg;
    return times[index % 2];
}

static double no_cost(instruments_context_t ctx, void *strategy_arg, void *chooser_arg)
{
    return 0.0;
}

static pthread_mutex_t mutex = MY_PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
instruments_strategy_t strategy;

static void store_chosen_strategy(instruments_strategy_t chosen, void *unused)
{
    pthread_mutex_lock(&mutex);
    strategy = chosen;
    pthread_cond_signal(&cv);
    pthread_mutex_unlock(&mutex);
}

static void
check_chosen_strategy_async(instruments_strategy_evaluator_t evaluator,
                            void *chooser_arg,
                            instruments_strategy_t correct_strategy)
{
    pthread_mutex_lock(&mutex);
    strategy = NULL;
    choose_strategy_async(evaluator, chooser_arg, store_chosen_strategy, NULL);
    while (strategy == NULL) {
        pthread_cond_wait(&cv, &mutex);
    }
    ASSERT_EQUAL((int)correct_strategy, (int) strategy);

    pthread_mutex_unlock(&mutex);
}

CTEST(async_test, arg_is_passed)
{
    instruments_strategy_t strategies[2];

    strategies[0] = make_strategy(one_of_two_values, NULL, no_cost, (void*) 0, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(one_of_two_values, NULL, no_cost, (void*) 1, NULL);
    ASSERT_NOT_NULL(strategies[1]);

    /* two strategies; which one wins depends entirely on the argument to choose_strategy. */
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set("", strategies, 2);
    instruments_strategy_t chosen;
    
    /* strategy 0 takes 5 seconds; strategy 1 takes 10 seconds */
    check_chosen_strategy_async(evaluator, (void*) 0, strategies[0]);
    
    /* strategy 0 takes 10 seconds; strategy 1 takes 5 seconds */
    check_chosen_strategy_async(evaluator, (void*) 1, strategies[1]);
}
