#ifndef ERROR_WEIGHT_PARAMS_H_INCL_BVAFOHVUFHERBVH0O
#define ERROR_WEIGHT_PARAMS_H_INCL_BVAFOHVUFHERBVH0O

#include <sys/types.h>

namespace instruments {
    extern const size_t MAX_SAMPLES;
    extern double NEW_SAMPLE_WEIGHT;
    extern double EWMA_GAIN;
    void update_ewma(double& ewma, double spot, double gain);
}

#endif

