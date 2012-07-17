#include "stats_distribution_binned.h"

void 
StatsDistributionBinned::addValue(double value)
{
    // TODO
}

double 
StatsDistributionBinned::Iterator::probability()
{
    // TODO
}

double
StatsDistributionBinned::Iterator::value()
{
    // TODO
}

void
StatsDistributionBinned::Iterator::advance()
{
    // TODO
}

bool
StatsDistributionBinned::Iterator::isDone()
{
    // TODO
}

StatsDistributionBinned::Iterator::Iterator(StatsDistributionBinned *d)
{
    // TODO
}

StatsDistribution::Iterator *
StatsDistributionBinned::makeNewIterator()
{
    return new StatsDistributionBinned::Iterator(this);
}
