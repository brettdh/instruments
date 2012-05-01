#include <stdlib.h>
#include "instruments.h"
#include "instruments_private.h"

instruments_strategy_t
make_strategy(eval_fn_t time_fn, /* return seconds */
              eval_fn_t energy_cost_fn, /* return Joules */
              eval_fn_t data_cost_fn, /* return bytes */
              void *arg)
{
    return NULL;
}
