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

    instruments_strategy_evaluator_t evaluator = register_strategy_set(strategies, 2);
    
    instruments_strategy_t chosen_strategy = choose_strategy(evaluator);
    ASSERT_EQUAL((int)strategies[1], (int)chosen_strategy);

    free_strategy_evaluator(evaluator);
    free_strategy(strategies[0]);
    free_strategy(strategies[1]);
}

static const int NUM_STRATEGIES = 3;

CTEST_DATA(coinflip) {
    instruments_strategy_t *strategies;
    instruments_strategy_evaluator_t evaluator;
};

CTEST_SETUP(coinflip)
{
    int i;
    reset_coin_flip_estimator(RUNNING_MEAN);
    
    data->strategies = calloc(NUM_STRATEGIES, sizeof(*data));
    data->strategies[0] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    data->strategies[1] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 0);
    data->strategies[2] = make_redundant_strategy(data->strategies, 2);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        ASSERT_NOT_NULL(data->strategies[i]);
    }

    data->evaluator = 
        register_strategy_set_with_method(data->strategies, NUM_STRATEGIES, EMPIRICAL_ERROR);
}

CTEST_TEARDOWN(coinflip)
{
    int i;
    free_strategy_evaluator(data->evaluator);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        free_strategy(data->strategies[i]);
    }
    free(data->strategies);
}

static void init_coin(int num_heads_in_ten_flips, int numflips)
{
    int i;
    for (i = 0; i < numflips; ++i) {
        if ((i % 10) < num_heads_in_ten_flips) {
            add_coin_flip_observation(1);
        } else {
            add_coin_flip_observation(0);
        }
    }
}

static void assert_correct_strategy(struct coinflip_data *data, 
                                    instruments_strategy_t correct_strategy)
{
    instruments_strategy_t *strategies = data->strategies;
    instruments_strategy_evaluator_t evaluator = data->evaluator;
    
    instruments_strategy_t chosen_strategy = choose_strategy(evaluator);
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
        const char *names[] = {"'pick heads'", "'pick tails'", "'pick both'"};
        CTEST_LOG("Failed to choose %s; chose %s instead\n", 
                  names[correct_strategy_idx],
                  names[chosen_strategy_idx]);
    }
    ASSERT_EQUAL((int)correct_strategy, (int)chosen_strategy);
}

CTEST2(coinflip, faircoin_should_choose_redundant)
{
    init_coin(5, 100);
    assert_correct_strategy(data, data->strategies[2]);
}

CTEST2(coinflip, heads_heavy_coin_should_choose_heads)
{
    init_coin(9, 100);
    assert_correct_strategy(data, data->strategies[0]);
}

CTEST2(coinflip, tails_heavy_coin_should_choose_tails)
{
    init_coin(1, 100);
    assert_correct_strategy(data, data->strategies[1]);
}

