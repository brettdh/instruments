#ifndef _INSTRUMENTS_PRIVATE_H_
#define _INSTRUMENTS_PRIVATE_H_

#ifdef __cplusplus
#define CDECL extern "C"
#else
#define CDECL
#endif

/* for testing. */
CDECL int coin_flip_lands_heads(instruments_context_t ctx);

#include "estimator_type.h"
CDECL void reset_coin_flip_estimator(enum EstimatorType type);
CDECL void add_coin_flip_observation(int heads);

#include "eval_method.h"
/* for testing, allows the caller to specify the method of estimator evaluation. */
CDECL instruments_strategy_evaluator_t
register_strategy_set_with_method(const char *name, const instruments_strategy_t *strategies, size_t num_strategies,
                                  enum EvalMethod type);

#include "estimator_range_hints.h"

#ifdef __cplusplus
class Estimator;
/* for testing, get the adjusted value of any estimator. */
double get_adjusted_estimator_value(instruments_context_t ctx, Estimator *estimator);
#endif

#endif /* _INSTRUMENTS_PRIVATE_H_ */
