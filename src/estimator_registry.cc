#include "estimator_registry.h"

// TODO: add an 'init' method that sets up all of the singleton estimators.

Estimator *
EstimatorRegistry::getNetworkBandwidthDownEstimator(const char *iface)
{
    // TODO: implement
    return NULL;
}

Estimator *
EstimatorRegistry::getNetworkBandwidthUpEstimator(const char *iface)
{
    // TODO: implement
    return NULL;
}

Estimator *
EstimatorRegistry::getNetworkRttEstimator(const char *iface)
{
    // TODO: implement
    return NULL;
}

Estimator *
EstimatorRegistry::getFairCoinEstimator()
{
    // TODO: implement
    return NULL;
}

Estimator *
EstimatorRegistry::getHeadsHeavyCoinEstimator()
{
    // TODO: implement
    return NULL;
}
