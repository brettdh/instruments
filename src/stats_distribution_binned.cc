#include "stats_distribution_binned.h"
#include <assert.h>
#include <sstream>
#include <string>
#include <iomanip>
#include <vector>
using std::ostringstream; using std::string; using std::hex;
using std::vector;

#include <RInside.h>
using Rcpp::as;

#include "r_singleton.h"

void
StatsDistributionBinned::initRSamplesName()
{
    ostringstream oss;
    oss << "samples_" << hex << this;
    r_samples_name = oss.str();
}

StatsDistributionBinned::StatsDistributionBinned()
{
    initRSamplesName();
}

StatsDistributionBinned::StatsDistributionBinned(vector<double> breaks_)
{
    initRSamplesName();

    breaks = breaks_;
    counts.resize(breaks.size() + 1, 0);
    mids.push_back(0.0);
    for (size_t i = 0; i < breaks.size() - 1; ++i) {
        double mid = (breaks[i] + breaks[i+1]) / 2.0;
        mids.push_back(mid);
    }
    mids.push_back(0.0);
    
    assertValidHistogram();
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
StatsDistributionBinned::Iterator::probability()
{
    return (double(distribution->counts[index]) / 
            distribution->all_samples_sorted.size());
}

double
StatsDistributionBinned::Iterator::value()
{
    return distribution->mids[index];
}

void
StatsDistributionBinned::Iterator::advance()
{
    if (index < (int) distribution->mids.size()) {
        ++index;
    }
}

bool
StatsDistributionBinned::Iterator::isDone()
{
    return (index == (int) distribution->mids.size());
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

RInside& StatsDistributionBinned::R = get_rinside_instance();
StatsDistributionBinned::initer StatsDistributionBinned::the_initer;

StatsDistributionBinned::initer::initer()
{
    R.parseEvalQ("library(histogram)");
}

void StatsDistributionBinned::calculateBins()
{
    assertValidHistogram();

    vector<double> samples(all_samples_sorted.begin(), all_samples_sorted.end());
    R[r_samples_name] = samples;

    ostringstream oss;
    oss << "histogram::histogram(" << r_samples_name << ", verbose=FALSE, plot=FALSE)";
    Rcpp::List hist_result = R.parseEval(oss.str());

    breaks = as<vector<double> >(hist_result["breaks"]);
    vector<double> tmp_mids = as<vector<double> >(hist_result["mids"]);
    vector<int> tmp_counts = as<vector<int> >(hist_result["counts"]);

    mids.assign(1, 0.0);
    mids.insert(mids.end(), tmp_mids.begin(), tmp_mids.end());
    mids.push_back(0.0);

    counts.assign(1, 0);
    counts.insert(counts.end(), tmp_counts.begin(), tmp_counts.end());
    counts.push_back(0);
    
    // tails are now empty, because all the data fits in the bins.

    oss.str("");
    oss << "remove(" << r_samples_name << ")";
    R.parseEvalQ(oss.str());

    assertValidHistogram();
}

bool
StatsDistributionBinned::binsAreSet()
{
    return (!breaks.empty());
}

bool
StatsDistributionBinned::shouldRebin()
{
    // TODO: periodic rebinning?
    return (all_samples_sorted.size() >= histogram_threshold &&
            !binsAreSet());
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
        vector<double>::iterator pos = 
            lower_bound(breaks.begin(), breaks.end(), value);
        size_t index = int(pos - breaks.begin());
        updateBin(index, value);
    } else {
        // no histogram yet; ignore.
    }
    all_samples_sorted.insert(value);

    assertValidHistogram();
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

        assert(counts[0] == 0 || mids[0] < breaks[0]);
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
