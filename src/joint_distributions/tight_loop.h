#ifndef TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU
#define TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU

// had to put this into a reusable macro in order to 
//  make a (relatively) performant 4-way loop.

//void IntNWJointDistribution::FN_BODY_WITH_COMBINER(double& weightedSum, double (*COMBINER)(double, double), size_t saved_value_type) {
#define FN_BODY_WITH_COMBINER(weightedSum, COMBINER, saved_value_type) \
    assert(wifi_strategy_saved_values != NULL);                  \
    assert(wifi_strategy_with_sessions_saved_values != NULL);                  \
    assert(cellular_strategy_saved_values != NULL);                  \
    assert(wifi_strategy_saved_values[saved_value_type] != NULL ||    \
           wifi_strategy_with_sessions_saved_values[saved_value_type] != NULL);     \
    assert(cellular_strategy_saved_values[saved_value_type] != NULL); \
                                                                                       \
    size_t max_i, max_j, max_k, max_m, max_n;  /* counts for: */              \
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */ \
    max_j = singular_samples_count[0][1]; /* (strategy 0, estimator 1) */            \
    if (wifi_uses_sessions) {                                           \
        max_k = singular_samples_count[0][2]; /* (strategy 0, estimator 2) */ \
    } else {                                                            \
        max_k = 1; /* essentially pruning that loop level; just run the inner loop */            \
    }                                                                   \
    max_m = singular_samples_count[1][0]; /* (strategy 1, estimator 3) */            \
    max_n = singular_samples_count[1][1]; /* (strategy 1, estimator 4) */            \
                                                                                   \
    double ***wifi_strategy_with_sessions_cur_saved_values = wifi_strategy_with_sessions_saved_values[saved_value_type]; \
    double **wifi_strategy_cur_saved_values = wifi_strategy_saved_values[saved_value_type]; \
    double **cellular_strategy_cur_saved_values = cellular_strategy_saved_values[saved_value_type];          \
                                                                        \
    for (size_t i = 0; i < max_i; ++i) {                                \
        double prob_i = singular_probabilities[0][0][i];                \
        double **tmp_i_with_sessions = NULL, *tmp_i = NULL;                           \
        if (wifi_uses_sessions) {                                       \
            tmp_i_with_sessions = wifi_strategy_with_sessions_cur_saved_values[i]; \
        } else {                                                        \
            tmp_i = wifi_strategy_cur_saved_values[i];                  \
        }                                                               \
        for (size_t j = 0; j < max_j; ++j) {                            \
            double prob_j = prob_i * singular_probabilities[0][1][j];   \
            double *tmp_j_with_sessions = NULL;                                \
            double tmp_wifi_strategy = DBL_MAX;                         \
            if (wifi_uses_sessions) {                                   \
                tmp_j_with_sessions = tmp_i_with_sessions[j];           \
            } else {                                                    \
                tmp_wifi_strategy = tmp_i[j];                           \
            }                                                           \
            for (size_t k = 0; k < max_k; ++k) {                        \
                double prob_k = 0.0;                                          \
                if (wifi_uses_sessions) {                               \
                    prob_k = prob_j * singular_probabilities[0][2][k];  \
                    tmp_wifi_strategy = tmp_j_with_sessions[k];         \
                } else {                                                \
                    prob_k = prob_j;                                    \
                }                                                       \
                assert(tmp_wifi_strategy != DBL_MAX);                   \
                for (size_t m = 0; m < max_m; ++m) {                    \
                    double prob_m = prob_k * singular_probabilities[1][0][m]; \
                    double *tmp_m = cellular_strategy_cur_saved_values[m];  \
                                                                        \
                    for (size_t n = 0; n < max_n; ++n) {                \
                        double tmp_cellular_strategy = tmp_m[n];        \
                        assert(tmp_cellular_strategy != DBL_MAX);       \
                                                                        \
                        double value = COMBINER(tmp_wifi_strategy, tmp_cellular_strategy); \
                        double probability = (prob_m * singular_probabilities[1][1][n]); \
                        weightedSum += (value * probability);           \
                    }                                                   \
                }                                                       \
            }                                                           \
        }                                                               \
    }

#endif
