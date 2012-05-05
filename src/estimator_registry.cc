#include "estimator.h"
#include "estimator_registry.h"

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
    return singletonEstimator("FairCoin");
}

Estimator *
EstimatorRegistry::getHeadsHeavyCoinEstimator()
{
    return singletonEstimator("HeadsHeavyCoin");
}


std::map<std::string, Estimator *> EstimatorRegistry::estimators;
EstimatorRegistry::initializer EstimatorRegistry::theInitializer;

void
EstimatorRegistry::init()
{
    estimators["FairCoin"] = Estimator::create();
    
    // TODO: move this into the testcase.
    getFairCoinEstimator()->addObservation(1.0);
    getFairCoinEstimator()->addObservation(0.0);

    estimators["HeadsHeavyCoin"] = Estimator::create();
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
    estimators[name] = Estimator::create(type);
}
