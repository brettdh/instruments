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
    int flip_result = fair_coin_lands_heads(ctx);
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

CTEST(coinflip, faircoin)
{
    int i, NUM_STRATEGIES = 3;
    instruments_strategy_t strategies[NUM_STRATEGIES];
    strategies[0] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    strategies[1] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 0);
    strategies[2] = make_redundant_strategy(strategies, 2);
    for (i = 0; i < NUM_STRATEGIES; ++i) {
        ASSERT_NOT_NULL(strategies[i]);
    }

    add_estimator(strategies[0], INSTRUMENTS_ESTIMATOR_FAIR_COIN);
    add_estimator(strategies[1], INSTRUMENTS_ESTIMATOR_FAIR_COIN);

    instruments_strategy_t chosen_strategy = choose_strategy(strategies, 3);
    ASSERT_EQUAL((int)strategies[2], (int)chosen_strategy);
}
