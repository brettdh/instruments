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
    switch (dist_type) {
    case ALL_SAMPLES_WEIGHTED:
        return new StatsDistributionAllSamples(true); // use weighted error method.
    case ALL_SAMPLES:
        return new StatsDistributionAllSamples(false);
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
