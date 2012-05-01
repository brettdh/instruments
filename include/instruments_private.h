#ifndef _INSTRUMENTS_PRIVATE_H_
#define _INSTRUMENTS_PRIVATE_H_

typedef void * instruments_context_t;
typedef void * instruments_strategy_t;

/* for testing. */
#define INSTRUMENTS_ESTIMATOR_FAIR_COIN 0x10001
int fair_coin_lands_heads(instruments_context_t ctx);

#endif /* _INSTRUMENTS_PRIVATE_H_ */
