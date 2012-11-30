#include "stats_distribution_all_samples.h"
#include <assert.h>

#include <string>
#include <fstream>
#include <stdexcept>
using std::string; using std::ifstream; using std::ofstream;
using std::runtime_error;

void 
StatsDistributionAllSamples::addValue(double value)
{
    values.push_back(value);
}

inline double 
StatsDistributionAllSamples::Iterator::probability()
{
    return cached_probability;
}

inline double 
StatsDistributionAllSamples::Iterator::probability(int pos)
{
    return cached_probability;
}

inline double
StatsDistributionAllSamples::Iterator::value()
{
    return at(cur_position);
}

inline double
StatsDistributionAllSamples::Iterator::at(int pos)
{
    return distribution->values[pos];
}

inline void
StatsDistributionAllSamples::Iterator::advance()
{
    ++cur_position;
}

inline bool
StatsDistributionAllSamples::Iterator::isDone()
{
    return (cur_position == total_count);
}

inline void
StatsDistributionAllSamples::Iterator::reset()
{
    cur_position = 0;
}

inline int
StatsDistributionAllSamples::Iterator::position()
{
    return cur_position;
}

inline int 
StatsDistributionAllSamples::Iterator::totalCount()
{
    return total_count;
}

StatsDistributionAllSamples::Iterator::Iterator(StatsDistributionAllSamples *d)
    : distribution(d), cur_position(0)
{
    assert(distribution->values.size() > 0);
    total_count = distribution->values.size();
    cached_probability = 1.0 / total_count;
}

StatsDistribution::Iterator *
StatsDistributionAllSamples::makeNewIterator()
{
    return new StatsDistributionAllSamples::Iterator(this);
}

void 
StatsDistributionAllSamples::appendToFile(const string& name, ofstream& out)
{
    throw runtime_error("NOT IMPLEMENTED");
}

void 
StatsDistributionAllSamples::restoreFromFile(const string& name, ifstream& in)
{
    throw runtime_error("NOT IMPLEMENTED");
}
