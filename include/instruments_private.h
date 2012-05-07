#ifndef _INSTRUMENTS_PRIVATE_H_
#define _INSTRUMENTS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* for testing. */
void add_coin_flip_estimator(instruments_strategy_t strategy);
int coin_flip_lands_heads(instruments_context_t ctx);

#include "estimator_type.h"
void reset_coin_flip_estimator(enum EstimatorType type);
void add_coin_flip_observation(int heads);

#include "eval_method.h"
/* for testing, allows the caller to specify the method of estimator evaluation. */
instruments_strategy_evaluator_t
register_strategy_set_with_method(const instruments_strategy_t *strategies, size_t num_strategies,
                                  enum EvalMethod type);


#ifdef __cplusplus
}
#endif

#endif /* _INSTRUMENTS_PRIVATE_H_ */
