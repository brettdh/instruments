#include "abstract_joint_distribution.h"

#include "stats_distribution_all_samples.h"
#ifndef ANDROID
#include "stats_distribution_binned.h"
#endif

#include <stdlib.h>
#include <stdexcept>

StatsDistribution *
AbstractJointDistribution::createSamplesDistribution()
{
    // TODO: create the Bayesian variants.
    switch (eval_method) {
    case ALL_SAMPLES:
        return new StatsDistributionAllSamples;
    case BINNED:
#ifdef ANDROID
        throw std::runtime_error("Binned distribution not yet supported.");
#else
        return new StatsDistributionBinned;
#endif
    default:
        abort();
    }
}
