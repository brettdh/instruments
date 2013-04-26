#ifndef TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU
#define TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU

// had to put this into a reusable macro in order to 
//  make a (relatively) performant 4-way loop.

// TODO-BAYESIAN: override this (or part of it) in a subclass to implement the 
// TODO-BAYESIAN: relevant part of the posterior distribution calculation.
// TOOD-BAYESIAN: specifically, I need to be able to replace the empirical
// TOOD-BAYESIAN: joint-probability calculation with the posterior probability calculation.
// TODO-BAYESIAN: I determined earlier today that the compiler is pretty good at
// TODO-BAYESIAN: caching intermediate values in the plain multiplication,
// TODO-BAYESIAN: so maybe I can push that into a function call.
#define FN_BODY_WITH_COMBINER(weightedSum, COMBINER,                    \
                              PROBABILITY_GETTER, JOINT_PROBABILITY_GETTER, saved_value_type) \
    for (size_t i = 0; i < singular_strategies.size(); ++i) {                      \
        assert(singular_strategy_saved_values[i] != NULL);                         \
        assert(singular_strategy_saved_values[i][saved_value_type] != NULL);      \
    }                                                                              \
                                                                                   \
    size_t max_i, max_j, max_k, max_m;  /* counts for: */                          \
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */            \
    max_j = singular_samples_count[0][1]; /* (strategy 0, estimator 1) */            \
    max_k = singular_samples_count[1][0]; /* (strategy 1, estimator 2) */            \
    max_m = singular_samples_count[1][1]; /* (strategy 1, estimator 3) */            \
                                                                                   \
    double **strategy_0_saved_values = singular_strategy_saved_values[0][saved_value_type];          \
    double **strategy_1_saved_values = singular_strategy_saved_values[1][saved_value_type];          \
                                                                        \
    for (size_t i = 0; i < max_i; ++i) {                                           \
        double prob_i = PROBABILITY_GETTER(singular_probabilities, 0, 0, i); \
        double *tmp_i = strategy_0_saved_values[i];                                \
        for (size_t j = 0; j < max_j; ++j) {                                       \
            double prob_j = prob_i * PROBABILITY_GETTER(singular_probabilities, 0, 1, j); \
            double tmp_strategy_0 = tmp_i[j];                                      \
            assert(tmp_strategy_0 != DBL_MAX);                                     \
            for (size_t k = 0; k < max_k; ++k) {                                   \
                double prob_k = prob_j * PROBABILITY_GETTER(singular_probabilities, 1, 0, k); \
                double *tmp_k = strategy_1_saved_values[k];             \
                for (size_t m = 0; m < max_m; ++m) {                               \
                    double tmp_strategy_1 = tmp_k[m];                              \
                    assert(tmp_strategy_1 != DBL_MAX);                             \
                                                                                   \
                    double value = COMBINER(tmp_strategy_0, tmp_strategy_1);       \
                    double probability = (prob_k * PROBABILITY_GETTER(singular_probabilities, 1, 1, m) * \
                                          JOINT_PROBABILITY_GETTER(i, j, k, m));   \
                    weightedSum += (value * probability);                          \
                }                                                                  \
            }                                                                      \
        }                                                                          \
    }


#endif
