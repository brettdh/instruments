#ifndef ESTIMATOR_REGISTRY_H_INCL
#define ESTIMATOR_REGISTRY_H_INCL

#include <map>
#include <string>

class Estimator;

class EstimatorRegistry {
  public:
    static Estimator *getNetworkBandwidthDownEstimator(const char *iface);
    static Estimator *getNetworkBandwidthUpEstimator(const char *iface);
    static Estimator *getNetworkRttEstimator(const char *iface);
    
    static Estimator *getCoinFlipEstimator();

    static void resetEstimator(const char *name, EstimatorType type);
  private:
    class initializer {
    public:
        initializer() {
            init();
        }
    };
    static initializer theInitializer;
    
    static void init();
    static Estimator *singletonEstimator(const char *name);
    static std::map<std::string, Estimator *> estimators;
};

#endif
