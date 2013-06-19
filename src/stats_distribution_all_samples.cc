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

static const double MIN_SIZE_THRESHOLD = 0.01;
static const size_t MAX_SAMPLES = 20;
static double NEW_SAMPLE_WEIGHT = 0.0;
static deque<double> normalizers;

static double calculateWeight(size_t sample_index, size_t num_samples)
{
    return pow(NEW_SAMPLE_WEIGHT, num_samples - sample_index);
}

static double
calculateWeightedProbability(size_t num_samples, double weight)
{
    return 1.0 / num_samples * weight;
}

static struct StaticIniter {
    StaticIniter() {
        NEW_SAMPLE_WEIGHT = pow(MIN_SIZE_THRESHOLD, 1.0/MAX_SAMPLES);
        normalizers.resize(MAX_SAMPLES + 1);
        normalizers[0] = strtod("NAN", NULL);
        for (size_t i = 1; i < normalizers.size(); ++i) {
            normalizers[i] = 0.0;
            for (size_t j = 0; j < i; ++j) {
                double weight = calculateWeight(j, i);
                normalizers[i] += calculateWeightedProbability(i, weight);
            }
        }
    }
} initer;


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
    if (values.size() == MAX_SAMPLES) {
        values.pop_front();
    }
    values.push_back(value);
}

double 
StatsDistributionAllSamples::getWeight(size_t sample_index)
{
    if (weighted_error) {
        size_t num_values = values.size();
        assert(num_values > 0);
        assert(num_values < normalizers.size());
        return calculateWeight(sample_index, num_values) / normalizers[num_values];
    } else {
        return 1.0;
    }
}

double 
StatsDistributionAllSamples::calculateProbability(size_t sample_index)
{
    return calculateWeightedProbability(values.size(), getWeight(sample_index));
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
        values.push_back(value);
    }
    return name;
}
