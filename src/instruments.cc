#include <stdlib.h>
#include "instruments.h"
#include "instruments_private.h"

struct instruments_strategy {
    eval_fn_t time_fn;
    eval_fn_t energy_cost_fn;
    eval_fn_t data_cost_fn;
    void *fn_arg;
};

instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *arg)
{
    struct instruments_strategy *strategy = malloc(sizeof(struct instruments_strategy));
    strategy->time_fn = time_fn;
    strategy->energy_cost_fn = energy_cost_fn;
    strategy->data_cost_fn = data_cost_fn;
    strategy->fn_arg = arg;
    return strategy;
}

void add_estimator(instruments_strategy_t strategy, 
                   estimator_id_t estimator_id)
{
    /* TODO */
}

void free_strategy(instruments_strategy_t strategy)
{
    free(strategy);
}

ssize_t
choose_strategy(const instruments_strategy_t *strategies, size_t num_strategies,
                /* OUT */ instruments_strategy_t *chosen_strategies)
{
    /* TODO */
    return 0;
}

int fair_coin_lands_heads(instruments_context_t ctx)
{
    /* TODO */
    return 0;
}
