#include "abstract_joint_distribution.h"

#include "stats_distribution_all_samples.h"
#ifndef ANDROID
#include "stats_distribution_binned.h"
#endif

#include <stdlib.h>
#include <stdexcept>

StatsDistribution *
AbstractJointDistribution::createSamplesDistribution(Estimator *estimator)
{
    // TODO: create the Bayesian variants.
    switch (dist_type) {
    case ALL_SAMPLES:
        return new StatsDistributionAllSamples;
    case BINNED:
#ifdef ANDROID
        throw std::runtime_error("Binned distribution not yet supported.");
#else
        return StatsDistributionBinned::create(estimator);
#endif
    default:
        abort();
    }
}
