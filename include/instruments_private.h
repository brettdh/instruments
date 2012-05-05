#ifndef _INSTRUMENTS_PRIVATE_H_
#define _INSTRUMENTS_PRIVATE_H_

#ifdef __cplusplus
extern "C" {
#endif

/* for testing. */
void add_fair_coin_estimator(instruments_strategy_t strategy);
void add_heads_heavy_coin_estimator(instruments_strategy_t strategy);
int fair_coin_lands_heads(instruments_context_t ctx);
int heads_heavy_coin_lands_heads(instruments_context_t ctx);

#include "estimator_type.h"
void reset_fair_coin_estimator(enum EstimatorType type);
void add_coin_flip_observation(int heads);

#include "eval_strategy.h"
/* for testing, allows the caller to specify the method of estimator evaluation. */
/* TODO: this method made me realize that the method needs to be specified 
 * TODO:  when a set of related strategies is registered, not the first time
 * TODO:  that the decision is needed. */
instruments_strategy_t
choose_strategy_by_method(const instruments_strategy_t *strategies, size_t num_strategies,
                          enum EvalStrategy type);


#ifdef __cplusplus
}
#endif

#endif /* _INSTRUMENTS_PRIVATE_H_ */
