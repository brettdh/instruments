#include "stats_distribution_binned.h"
#include "estimator.h"

#include <assert.h>
#include <sstream>
#include <fstream>
#include <string>
#include <iomanip>
#include <vector>
#include <stdexcept>
using std::ostringstream; using std::string; using std::hex;
using std::vector; using std::ifstream; using std::ofstream;
using std::runtime_error;

#include "small_set.h"
#include "timeops.h"

#ifndef ANDROID
#include <RInside.h>
using Rcpp::as;

#include "r_singleton.h"

void
StatsDistributionBinned::initRInside()
{
    R = get_rinside_instance();
    R->parseEvalQ("library(histogram)");

    ostringstream oss;
    oss << "samples_" << hex << this;
    r_samples_name = oss.str();
}
#endif

class NoRInsideOnAndroid : public runtime_error {
  public:
    NoRInsideOnAndroid() :
        runtime_error("Can't auto-determine histogram bins on Android. (no R)") {}
};

StatsDistributionBinned::StatsDistributionBinned()
{
#ifdef ANDROID
    throw NoRInsideOnAndroid();
#else
    initRInside();
    preset_breaks = false;
#endif
}

StatsDistributionBinned::StatsDistributionBinned(vector<double> new_breaks)
{
    setBreaks(new_breaks);
    
    assertValidHistogram();
}

void
StatsDistributionBinned::setBreaks(const vector<double>& new_breaks)
{
    preset_breaks = true;
    breaks = new_breaks;
    counts.resize(breaks.size() + 1, 0);
    mids.clear();
    mids.push_back(0.0);
    for (size_t i = 0; i < breaks.size() - 1; ++i) {
        double mid = (breaks[i] + breaks[i+1]) / 2.0;
        mids.push_back(mid);
    }
    mids.push_back(0.0);
}

// construct a regular histogram with num_bins bins ranging from min to max.
// e.g. min=0, max=10, num_bins=4 results in 
//      breaks: [0, 2.5, 5, 7.5, 10], the endpoints of 4 bins.
StatsDistributionBinned::StatsDistributionBinned(double min, double max, size_t num_bins)
{
    vector<double> new_breaks;
    double width = (max - min) / num_bins;
    for (size_t i = 0; i <= num_bins; ++i) {
        new_breaks.push_back(min + i * width);
    }
    setBreaks(new_breaks);
}

StatsDistributionBinned *
StatsDistributionBinned::create(Estimator *estimator)
{
    if (estimator && estimator->hasRangeHints()) {
        EstimatorRangeHints hints = estimator->getRangeHints();
        return new StatsDistributionBinned(hints.min, hints.max, hints.num_bins);
    } else {
        return new StatsDistributionBinned;
    }
}


void 
StatsDistributionBinned::addValue(double value)
{
    all_samples.addValue(value);
    addToHistogram(value);
    if (shouldRebin()) {
        calculateBins();
    }
}

double 
StatsDistributionBinned::getProbability(double value)
{
    if (binsAreSet()) {
        return probabilityAtIndex(getIndex(value));
    } else {
        return all_samples.getProbability(value);
    }
}

double 
StatsDistributionBinned::getBinnedValue(double value)
{
    if (binsAreSet()) {
        return mids[getIndex(value)];
    } else {
        return value;
    }
}


double
StatsDistributionBinned::probabilityAtIndex(size_t index)
{
    assert(index < counts.size());
    return (double(counts[index]) / all_samples_sorted.size());
}

double 
StatsDistributionBinned::Iterator::probability()
{
    return probability(index);
}

inline double 
StatsDistributionBinned::Iterator::probability(int pos)
{
    return distribution->probabilityAtIndex(pos);
}

inline double
StatsDistributionBinned::Iterator::value()
{
    return at(index);
}

inline double
StatsDistributionBinned::Iterator::at(int pos)
{
    return distribution->mids[pos];
}

inline void
StatsDistributionBinned::Iterator::advance()
{
    if (index < (int) distribution->mids.size()) {
        ++index;
    }
}

inline bool
StatsDistributionBinned::Iterator::isDone()
{
    return (index == (int) distribution->mids.size());
}

inline void
StatsDistributionBinned::Iterator::reset()
{
    index = 0;
}

inline int
StatsDistributionBinned::Iterator::position()
{
    return index;
}

inline int
StatsDistributionBinned::Iterator::totalCount()
{
    return (int) distribution->mids.size();
}

StatsDistributionBinned::Iterator::Iterator(StatsDistributionBinned *d)
{
    distribution = d;
    index = 0;
}

StatsDistribution::Iterator *
StatsDistributionBinned::makeNewIterator()
{
    if (breaks.empty()) {
        return all_samples.getIterator();
    } else {
        return new StatsDistributionBinned::Iterator(this);
    }
}

void mark_timepoint(const char *msg)
{
    static std::string last_msg;
    static struct timeval last_timestamp = {0, 0};

    struct timeval now;
    TIME(now);
    
    if (last_msg.length() > 0) {
        struct timeval diff;
        TIMEDIFF(last_timestamp, now, diff);
        fprintf(stderr, "%s took %lu.%06lu seconds\n",
                last_msg.c_str(), diff.tv_sec, diff.tv_usec);
    }
    if (msg) {
        last_msg.assign(msg);
    } else {
        last_msg.assign("");
    }
    last_timestamp = now;
}

void StatsDistributionBinned::calculateBins()
{
#ifdef ANDROID
    throw NoRInsideOnAndroid();
#else
    assertValidHistogram();

    vector<double> samples(all_samples_sorted.begin(), all_samples_sorted.end());
    std::set<double> unique_samples(samples.begin(), samples.end());
    if (unique_samples.size() <= 1) {
        // R's histogram function blows up if I pass a vector of one value
        // try again next sample
        return;
    }

    R->assign(samples, r_samples_name);
    //R->parseEval(string("print(") + r_samples_name + string(")"));

    //mark_timepoint("calling histogram()");
    ostringstream oss;
    oss << "histogram::histogram(" << r_samples_name << ", verbose=FALSE, plot=FALSE)";
    Rcpp::List hist_result = R->parseEval(oss.str());
    //mark_timepoint(NULL);

    breaks = as<vector<double> >(hist_result["breaks"]);
    vector<double> tmp_mids = as<vector<double> >(hist_result["mids"]);
    vector<int> tmp_counts = as<vector<int> >(hist_result["counts"]);
    //fprintf(stderr, "histogram with %zu bins\n", tmp_counts.size());

    mids.assign(1, 0.0);
    mids.insert(mids.end(), tmp_mids.begin(), tmp_mids.end());
    mids.push_back(0.0);

    counts.assign(1, 0);
    counts.insert(counts.end(), tmp_counts.begin(), tmp_counts.end());
    counts.push_back(0);
    
    // tails are now empty, because all the data fits in the bins.

    oss.str("");
    oss << "remove(" << r_samples_name << ")";
    R->parseEvalQ(oss.str());

    assertValidHistogram();
    
    //printHistogram();
#endif
}

void
StatsDistributionBinned::printHistogram()
{
    fprintf(stderr, "breaks   ltail  |");
    for (size_t i = 0; i < breaks.size(); ++i) {
        fprintf(stderr, "%-8.3f|", breaks[i]);
    }
    fprintf(stderr, "rtail\n");
    
    fprintf(stderr, "counts   %5d   ", counts[0]);
    for (size_t i = 1; i < counts.size(); ++i) {
        fprintf(stderr, "%8d ", counts[i]);
    }
    fprintf(stderr, "\n");
    
    fprintf(stderr, "mids     %5f   ", mids[0]);
    for (size_t i = 1; i < mids.size(); ++i) {
        fprintf(stderr, "%8f ", mids[i]);
    }
    fprintf(stderr, "\n");
}

bool
StatsDistributionBinned::binsAreSet()
{
    // because I shouldn't be making histograms without range hints anymore.
    assert(!breaks.empty());
    
    return (!breaks.empty());
}

bool
StatsDistributionBinned::shouldRebin()
{
    return (!preset_breaks && (all_samples_sorted.size() % histogram_threshold) == 0);
}

void
StatsDistributionBinned::addToHistogram(double value)
{
    // breaks:   0   1   2   3   ...
    // mids:   0   1   2   3   4 ...
    // counts: 0   1   2   3   4 ...
    
    // if a value falls between breaks n and n+1,
    //  lower_bound returns n+1.
    // When the value is within the histogram, this returns something
    //  between 1 and n.
    // When the value is less than the first break,
    //  it returns position 0, which points to the "left tail" bin.
    // When the value is greater than the last break,
    //  it returns position numbins+1, which points to the "right tail" bin.
    
    assertValidHistogram();
    
    if (binsAreSet()) {
        size_t index = getIndex(value);
        updateBin(index, value);
    } else {
        // no histogram yet; ignore.
    }
    all_samples_sorted.insert(value);

    assertValidHistogram();
}

size_t
StatsDistributionBinned::getIndex(double value)
{
    vector<double>::iterator pos = 
        lower_bound(breaks.begin(), breaks.end(), value);
    size_t index = int(pos - breaks.begin());
    return index;
}

void
StatsDistributionBinned::updateBin(int index, double value)
{
    if (index == 0 || index == (int) breaks.size()) {
        addToTail(counts[index], mids[index], value);
    } else {
        counts[index]++;
    }
}

void
StatsDistributionBinned::assertValidHistogram()
{
    if (!breaks.empty()) {
        assert((breaks.size() + 1) == mids.size());
        assert((breaks.size() + 1) == counts.size());

        int total_counts = 0;
        for (size_t i = 1; i < breaks.size(); ++i) {
            assert(breaks[i-1] <= mids[i]);
            assert(breaks[i] > mids[i]);
            
            total_counts += counts[i];
        }

        assert(counts[0] == 0 || mids[0] <= breaks[0]);
        assert(counts[counts.size()-1] == 0 ||
               mids[mids.size()-1] > breaks[breaks.size()-1]);

        total_counts += counts[0];
        total_counts += counts[counts.size() - 1];
        assert(total_counts == (int) all_samples_sorted.size());
    }
}

void
StatsDistributionBinned::addToTail(int& count, double& mid, double value)
{
    double sum = (count * mid);
    count++;
    mid = (sum + value) / count;
}

void 
StatsDistributionBinned::appendToFile(const string& name, ofstream& out)
{
    throw runtime_error("NOT IMPLEMENTED");
}

string
StatsDistributionBinned::restoreFromFile(ifstream& in)
{
    throw runtime_error("NOT IMPLEMENTED");
}
