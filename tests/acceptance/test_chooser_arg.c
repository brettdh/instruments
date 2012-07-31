#include <instruments.h>
#include "ctest.h"

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


CTEST(chooser_arg, arg_is_passed)
{
    instruments_strategy_t strategies[2];

    strategies[0] = make_strategy(one_of_two_values, NULL, no_cost, (void*) 0, NULL);
    ASSERT_NOT_NULL(strategies[0]);
    strategies[1] = make_strategy(one_of_two_values, NULL, no_cost, (void*) 1, NULL);
    ASSERT_NOT_NULL(strategies[1]);

    /* two strategies; which one wins depends entirely on the argument to choose_strategy. */
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set(strategies, 2);
    instruments_strategy_t chosen;
    
    /* strategy 0 takes 5 seconds; strategy 1 takes 10 seconds */
    chosen = choose_strategy(evaluator, (void*) 0);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[0], (int) chosen);
    
    /* strategy 0 takes 10 seconds; strategy 1 takes 5 seconds */
    chosen = choose_strategy(evaluator, (void*) 1);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategies[1], (int) chosen);
}
