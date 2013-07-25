#ifndef TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU
#define TIGHT_LOOP_H_INCL_HGG0YHFGUOHEARVU

// had to put this into a reusable macro in order to 
//  make a (relatively) performant 4-way loop.

#ifdef WIFI_SESSION_LENGTH_ESTIMATOR
#define DECLARE_WIFI_SESSION_LENGTH_SAMPLE_COUNT() \
    size_t max_k = singular_samples_count[0][2]; /* (strategy 0, estimator 2) */

#define WIFI_SAVED_VALUE_TYPE double ***

#define WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_HEADER2()                     \
    double *tmp_j = tmp_i[j];                                           \
    for (size_t k = 0; k < max_k; ++k) {                                \
    double prob_wifi = prob_j * singular_probabilities[0][2][k];        \
    double tmp_wifi_strategy = tmp_j[k];                                \
    assert(tmp_wifi_strategy != DBL_MAX);

#define WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_FOOTER()                     \
    }

#else
#define DECLARE_WIFI_SESSION_LENGTH_SAMPLE_COUNT()

#define WIFI_SAVED_VALUE_TYPE double **

#define WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_HEADER2() \
    double prob_wifi = prob_j;                                      \
    double tmp_wifi_strategy = tmp_i[j];

#define WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_FOOTER()

#endif



//void IntNWJointDistribution::FN_BODY_WITH_COMBINER(double& weightedSum, double (*COMBINER)(double, double), size_t saved_value_type) {
#define FN_BODY_WITH_COMBINER(weightedSum, COMBINER, saved_value_type) \
    assert(wifi_strategy_saved_values != NULL);                  \
    assert(cellular_strategy_saved_values != NULL);                  \
    assert(wifi_strategy_saved_values[saved_value_type] != NULL); \
    assert(cellular_strategy_saved_values[saved_value_type] != NULL); \
                                                                                       \
    size_t max_i, max_j, max_m, max_n;  /* counts for: */         \
    max_i = singular_samples_count[0][0]; /* (strategy 0, estimator 0) */ \
    max_j = singular_samples_count[0][1]; /* (strategy 0, estimator 1) */            \
    DECLARE_WIFI_SESSION_LENGTH_SAMPLE_COUNT();                         \
    max_m = singular_samples_count[1][0]; /* (strategy 1, estimator 3) */            \
    max_n = singular_samples_count[1][1]; /* (strategy 1, estimator 4) */            \
                                                                                   \
    WIFI_SAVED_VALUE_TYPE wifi_strategy_cur_saved_values = wifi_strategy_saved_values[saved_value_type]; \
    double **cellular_strategy_cur_saved_values = cellular_strategy_saved_values[saved_value_type];          \
                                                                        \
    for (size_t i = 0; i < max_i; ++i) {                                \
        double prob_i = singular_probabilities[0][0][i];                \
        auto tmp_i = wifi_strategy_cur_saved_values[i]; \
        for (size_t j = 0; j < max_j; ++j) {                            \
            double prob_j = prob_i * singular_probabilities[0][1][j];   \
            WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_HEADER2();                       \
            for (size_t m = 0; m < max_m; ++m) {                        \
                double prob_m = prob_wifi * singular_probabilities[1][0][m]; \
                double *tmp_m = cellular_strategy_cur_saved_values[m];  \
                                                                        \
                for (size_t n = 0; n < max_n; ++n) {                    \
                    double tmp_cellular_strategy = tmp_m[n];            \
                    assert(tmp_cellular_strategy != DBL_MAX);           \
                                                                        \
                    double value = COMBINER(tmp_wifi_strategy, tmp_cellular_strategy); \
                    double probability = (prob_m * singular_probabilities[1][1][n]); \
                    weightedSum += (value * probability);               \
                }                                                       \
            }                                                           \
            WIFI_SESSION_LENGTH_ESTIMATOR_LOOP_FOOTER();                \
        }                                                               \
    }

#endif
