#include <assert.h>
#include "estimator.h"
#include "estimator_registry.h"

Estimator *
EstimatorRegistry::getNetworkBandwidthDownEstimator(const char *iface)
{
    // TODO: implement
    assert(false);
    return NULL;
}

Estimator *
EstimatorRegistry::getNetworkBandwidthUpEstimator(const char *iface)
{
    // TODO: implement
    assert(false);
    return NULL;
}

Estimator *
EstimatorRegistry::getNetworkRttEstimator(const char *iface)
{
    // TODO: implement
    assert(false);
    return NULL;
}

Estimator *
EstimatorRegistry::getCoinFlipEstimator()
{
    return singletonEstimator("CoinFlip");
}


std::map<std::string, Estimator *> EstimatorRegistry::estimators;
EstimatorRegistry::initializer EstimatorRegistry::theInitializer;

void
EstimatorRegistry::init()
{
    estimators["CoinFlip"] = Estimator::create("CoinFlip");
}

Estimator *
EstimatorRegistry::singletonEstimator(const char *name)
{
    if (estimators.count(name) > 0) {
        return estimators[name];
    } else {
        return NULL;
    }
}

void
EstimatorRegistry::resetEstimator(const char *name, EstimatorType type)
{
    if (estimators.count(name) > 0) {
        delete estimators[name];
    }
    estimators[name] = Estimator::create(type, name);
}
