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
    int i;
    instruments_strategy_t
        strategies[2], 
        chosen_strategies[2] = {NULL, NULL};
    strategies[0] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 1);
    strategies[1] = make_strategy(coinflip_time, NULL, coinflip_data, (void*) 0);
    ASSERT_NOT_NULL(strategies[0]);
    ASSERT_NOT_NULL(strategies[1]);

    add_estimator(strategies[0], INSTRUMENTS_ESTIMATOR_FAIR_COIN);
    add_estimator(strategies[1], INSTRUMENTS_ESTIMATOR_FAIR_COIN);

    ssize_t num_chosen = choose_strategy(strategies, 2, chosen_strategies);
    ASSERT_EQUAL(2, num_chosen);
    for (i = 0; i < 2; ++i) {
        ASSERT_NOT_NULL(chosen_strategies[i]);
        ASSERT_EQUAL((int)strategies[i], (int)chosen_strategies[i]);
    }
}
