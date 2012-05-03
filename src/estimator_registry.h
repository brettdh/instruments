#ifndef ESTIMATOR_REGISTRY_H_INCL
#define ESTIMATOR_REGISTRY_H_INCL

class EstimatorRegistry {
  public:
    static Estimator *getNetworkBandwidthDownEstimator(const char *iface);
    static Estimator *getNetworkBandwidthUpEstimator(const char *iface);
    static Estimator *getNetworkRttEstimator(const char *iface);
    
    static Estimator *getFairCoinEstimator();
    static Estimator *getHeadsHeavyCoinEstimator();
};

#endif
