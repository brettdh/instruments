#include "stats_distribution_all_samples.h"
#include "debug.h"
#include <assert.h>

#include <string>
#include <fstream>
#include <stdexcept>
#include <iomanip>
#include <algorithm>
using std::string; using std::ifstream; using std::ofstream;
using std::endl; using std::setprecision;
using std::runtime_error; using std::count;

double
StatsDistributionAllSamples::getProbability(double value)
{
    return ((double) count(values.begin(), values.end(), value)) / values.size();
}

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
    ASSERT(distribution->values.size() > 0);
    total_count = distribution->values.size();
    cached_probability = 1.0 / total_count;
}

StatsDistribution::Iterator *
StatsDistributionAllSamples::makeNewIterator()
{
    return new StatsDistributionAllSamples::Iterator(this);
}

#define VALUES_PER_LINE 5
static const string TAG = "all-samples";

static int PRECISION = 20;

void 
StatsDistributionAllSamples::appendToFile(const string& name, ofstream& out)
{
    out << name << " " << TAG << " " << values.size() << endl;;
    for (size_t i = 0; i < values.size(); ++i) {
        out << setprecision(PRECISION) << values[i] << " ";
        if ((i+1) % VALUES_PER_LINE == 0 || i+1 == values.size()) {
            out << endl;
        }
    }
    check(out, "Failed to write values");
}

string
StatsDistributionAllSamples::restoreFromFile(ifstream& in)
{
    string name, type;
    int num_values = 0;
    check(in >> name >> type >> num_values, "Failed to read init fields");
    check(type == TAG, "Distribution type mismatch");

    values.clear();
    for (int i = 0; i < num_values; ++i) {
        double value = 0.0;
        check(in >> value, "Failed to read value");
        values.push_back(value);
    }
    return name;
}
