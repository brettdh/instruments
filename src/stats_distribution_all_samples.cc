#include "stats_distribution_all_samples.h"
#include "debug.h"

static const size_t MAX_SAMPLES = 1500;

void 
StatsDistributionAllSamples::addValue(double value)
{
    // TODO: think more about this.
    if (values.size() == MAX_SAMPLES) {
        values.pop_front();
    }

    values.push_back(value);
}

inline double 
StatsDistributionAllSamples::Iterator::probability()
{
    return cached_probability;
}

inline double
StatsDistributionAllSamples::Iterator::value()
{
    return *real_iterator;
}

inline void
StatsDistributionAllSamples::Iterator::advance()
{
    ++real_iterator;
}

inline bool
StatsDistributionAllSamples::Iterator::isDone()
{
    return (real_iterator == distribution->values.end());
}

inline void
StatsDistributionAllSamples::Iterator::reset()
{
    real_iterator = distribution->values.begin();
}

StatsDistributionAllSamples::Iterator::Iterator(StatsDistributionAllSamples *d)
    : distribution(d), real_iterator(d->values.begin())
{
    ASSERT(distribution->values.size() > 0);
    cached_probability = 1.0 / distribution->values.size();
}

StatsDistribution::Iterator *
StatsDistributionAllSamples::makeNewIterator()
{
    return new StatsDistributionAllSamples::Iterator(this);
}
