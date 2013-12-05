#ifndef TIGHT_LOOP_REMOTE_EXEC_H_INCL_GUV0EHJF0WHF09HA0B49W
#define TIGHT_LOOP_REMOTE_EXEC_H_INCL_GUV0EHJF0WHF09HA0B49W

// had to put this into a reusable macro in order to 
//  make a (relatively) performant 3-way loop.
// (I think. Just copying this to reuse with remote-exec.)

#ifdef DEBUG_REMOTE_EXEC_LOOP
void RemoteExecJointDistribution::FN_BODY_WITH_COMBINER(double& weightedSum, double (*COMBINER)(double, double), size_t saved_value_type) {
#endif

#define FN_BODY_WITH_COMBINER(weightedSum, COMBINER, saved_value_type) \
    assert(local_strategy_saved_values != NULL);                  \
    assert(remote_strategy_saved_values != NULL);                  \
    assert(local_strategy_saved_values[saved_value_type] != NULL); \
    assert(remote_strategy_saved_values[saved_value_type] != NULL);     \
                                                                                       \
    const size_t REMOTE_WIFI_BW_INDEX = 0;                              \
    const size_t REMOTE_WIFI_RTT_INDEX = 1;                              \
    size_t max_i, max_j, max_k, max_m;  /* counts for: */              \
    max_i = singular_samples_count[0][0]; /* strategy 0, estimator 0 (size) */ \
    max_j = singular_samples_count[1][2]; /* strategy 1, estimator 0 (size) */            \
    max_k = singular_samples_count[1][REMOTE_WIFI_BW_INDEX]; /* strategy 1, estimator 1 (wifi-bw) */ \
    max_m = singular_samples_count[1][REMOTE_WIFI_RTT_INDEX]; /* strategy 1, estimator 2 (wifi-rtt) */            \
    ASSERT(max_i == max_j);                                            \
    (void)max_j;                                                        \
                                                                                   \
    double *local_strategy_cur_saved_values = local_strategy_saved_values[saved_value_type];          \
    double ***remote_strategy_cur_saved_values = remote_strategy_saved_values[saved_value_type]; \
                                                                        \
    for (size_t i = 0; i < max_i; ++i) {                                \
        double prob_i = singular_probabilities[0][0][i];                \
        double tmp_local_strategy = local_strategy_cur_saved_values[i]; \
        /*double **tmp_i_remote = remote_strategy_cur_saved_values[i];  */ \
        assert(tmp_local_strategy != DBL_MAX);                          \
        for (size_t k = 0; k < max_k; ++k) {                            \
            double prob_k = prob_i * singular_probabilities[1][REMOTE_WIFI_BW_INDEX][k];   \
            double **tmp_k = remote_strategy_cur_saved_values[k];                            \
            for (size_t m = 0; m < max_m; ++m) {                        \
                double tmp_remote_strategy = tmp_k[m][i];                  \
                assert(tmp_remote_strategy != DBL_MAX);                 \
                                                                        \
                double value = COMBINER(tmp_local_strategy, tmp_remote_strategy); \
                double probability = (prob_k * singular_probabilities[1][REMOTE_WIFI_RTT_INDEX][m]); \
                weightedSum += (value * probability);                   \
            }                                                           \
        }                                                               \
    }

#ifdef DEBUG_REMOTE_EXEC_LOOP
}
#endif

#endif
