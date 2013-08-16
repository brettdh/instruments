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
using std::deque;

#include "error_weight_params.h"
using instruments::NEW_SAMPLE_WEIGHT;
using instruments::MAX_SAMPLES;
using instruments::calculate_sample_weight;
using instruments::calculate_weighted_probability;
using instruments::calculate_normalized_sample_weight;


StatsDistributionAllSamples::StatsDistributionAllSamples(bool weighted_error_)
    : weighted_error(weighted_error_)
{
}

double
StatsDistributionAllSamples::getProbability(double value)
{
    double prob = 0.0;
    for (size_t i = 0; i < values.size(); ++i) {
        if (values[i] == value) {
            prob += probabilityAtPosition(i);
        }
    }
    return prob;
}

void 
StatsDistributionAllSamples::addValue(double value)
{
    if (weighted_error && values.size() == MAX_SAMPLES) {
        values.pop_front();
    }
    values.push_back(value);
}

double 
StatsDistributionAllSamples::getWeight(size_t sample_index)
{
    if (weighted_error) {
        return calculate_normalized_sample_weight(sample_index, values.size());
    } else {
        return 1.0;
    }
}

double 
StatsDistributionAllSamples::calculateProbability(size_t sample_index)
{
    return calculate_weighted_probability(values.size(), getWeight(sample_index));
}

double
StatsDistributionAllSamples::probabilityAtPosition(size_t pos)
{
    return calculateProbability(pos);
}

inline double 
StatsDistributionAllSamples::Iterator::probability()
{
    return probability(cur_position);
}

inline double 
StatsDistributionAllSamples::Iterator::probability(size_t pos)
{
    return cached_probability[pos];
}

inline double
StatsDistributionAllSamples::Iterator::value()
{
    return at(cur_position);
}

inline double
StatsDistributionAllSamples::Iterator::at(size_t pos)
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
    cached_probability.resize(total_count);
    
    for (size_t i = 0; i < total_count; ++i) {
        cached_probability[i] = distribution->probabilityAtPosition(i);
    }
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
        addValue(value);
    }
    return name;
}
