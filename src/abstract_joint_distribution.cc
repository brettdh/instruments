#include "abstract_joint_distribution.h"

#include "stats_distribution_all_samples.h"
#include "stats_distribution_binned.h"

#include <stdlib.h>

StatsDistribution *
AbstractJointDistribution::createErrorDistribution()
{
    switch (eval_method) {
    case ALL_SAMPLES:
        return new StatsDistributionAllSamples;
    case BINNED:
        return new StatsDistributionBinned;
    default:
        abort();
    }
}
