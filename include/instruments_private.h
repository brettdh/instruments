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

#ifdef __cplusplus
}
#endif

#endif /* _INSTRUMENTS_PRIVATE_H_ */
