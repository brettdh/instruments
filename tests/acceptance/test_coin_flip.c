#include <instruments.h>
#include <instruments_private.h>

#include "ctest.h"

/* This test case simulates the scenario that Jason outlined
 *  in his MobiHeld keynote:
 * 
 *   You're calling the outcome of a coin toss.
 *   $10 if you get it right.
 *   $4 to pick heads, $4 to pick tails.
 *   What do you pick?
 *     If both are equally likely, pick both.
 *     If one is much more likely, pick that one.
 */

static double coinflip_time(instruments_context_t ctx, void *arg)
{
    int pick_heads = (int)arg;
    int flip_result = coin_flip_lands_heads(ctx);
    if ((pick_heads && flip_result) ||
        (!pick_heads && !flip_result)) {
        return -10.0;
    } else {
        return 0.0;
    }
}

static double coinflip_data(instruments_context_t ctx, void *arg)
{
    return 4.0;
}

CTEST(coinflip, one_strategy)
{
    reset_coin_flip_estimator(RUNNING_MEAN);
    add_coin_flip_observation(0);

    instruments_strategy_t strategy =
        make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    ASSERT_NOT_NULL(strategy);
    add_coin_flip_estimator(strategy);
    
    instruments_strategy_evaluator_t evaluator = register_strategy_set(&strategy, 1);
    instruments_strategy_t chosen = choose_strategy(evaluator);
    ASSERT_NOT_NULL(chosen);
    ASSERT_EQUAL((int)strategy, (int)chosen);

    free_strategy_evaluator(evaluator);
    free_strategy(strategy);
}

CTEST(coinflip, two_strategies)
{
    reset_coin_flip_estimator(RUNNING_MEAN);
    add_coin_flip_observation(1);
    add_coin_flip_observation(0);
    add_coin_flip_observation(1);

    int i, NUM_STRATEGIES = 2;
    instruments_strategy_t strategies[NUM_STRATEGIES];
    strategies[0] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 0);
    strategies[1] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        ASSERT_NOT_NULL(strategies[i]);
    }

    add_coin_flip_estimator(strategies[0]);
    add_coin_flip_estimator(strategies[1]);

    instruments_strategy_evaluator_t evaluator = register_strategy_set(strategies, 2);
    
    instruments_strategy_t chosen_strategy = choose_strategy(evaluator);
    ASSERT_EQUAL((int)strategies[1], (int)chosen_strategy);

    free_strategy_evaluator(evaluator);
    free_strategy(strategies[0]);
    free_strategy(strategies[1]);
}

CTEST(coinflip, faircoin_should_choose_redundant)
{
    reset_coin_flip_estimator(RUNNING_MEAN);
    add_coin_flip_observation(1);
    add_coin_flip_observation(0);
    add_coin_flip_observation(1);
    add_coin_flip_observation(0);
    
    int i, NUM_STRATEGIES = 3;
    instruments_strategy_t strategies[NUM_STRATEGIES];
    strategies[0] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    strategies[1] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 0);
    strategies[2] = make_redundant_strategy(strategies, 2);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        ASSERT_NOT_NULL(strategies[i]);
    }

    add_coin_flip_estimator(strategies[0]);
    add_coin_flip_estimator(strategies[1]);
    
    instruments_strategy_evaluator_t evaluator = 
        register_strategy_set_with_method(strategies, 3, EMPIRICAL_ERROR);

    instruments_strategy_t chosen_strategy = choose_strategy(evaluator);
    if (chosen_strategy != strategies[2]) {
        ASSERT_TRUE(chosen_strategy == strategies[0] ||
                    chosen_strategy == strategies[1]);
        
        const char *names[] = {"'pick heads'", "'pick tails'"};
        int idx = (chosen_strategy == strategies[0] ? 0 : 1);
        CTEST_LOG("Failed to choose redundant strategy; chose %s instead\n", names[idx]);
        ASSERT_FAIL();
    }
    ASSERT_EQUAL((int)strategies[2], (int)chosen_strategy);

    free_strategy_evaluator(evaluator);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(strategies[i]);
    }
}
