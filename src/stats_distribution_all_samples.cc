#include "stats_distribution_all_samples.h"

void 
StatsDistributionAllSamples::addValue(double value)
{
    values.push_back(value);
}

double 
StatsDistributionAllSamples::Iterator::probability()
{
    return 1.0 / distribution->values.size();
}

double
StatsDistributionAllSamples::Iterator::value()
{
    return *real_iterator;
}

void
StatsDistributionAllSamples::Iterator::advance()
{
    ++real_iterator;
}

bool
StatsDistributionAllSamples::Iterator::isDone()
{
    return (real_iterator == distribution->values.end());
}

StatsDistributionAllSamples::Iterator::Iterator(StatsDistributionAllSamples *d)
    : distribution(d), real_iterator(d->values.begin())
{
}

StatsDistribution::Iterator *
StatsDistributionAllSamples::makeNewIterator()
{
    return new StatsDistributionAllSamples::Iterator(this);
}
