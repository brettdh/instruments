#ifndef STATS_DISTRIBUTION_BINNED_H_INCL
#define STATS_DISTRIBUTION_BINNED_H_INCL

#include <vector>
#include <set>
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"

class RInside;

class Estimator;

class StatsDistributionBinned : public StatsDistribution {
  public:
    StatsDistributionBinned(bool weighted_error_ = false);
    StatsDistributionBinned(std::vector<double> breaks, bool weighted_error_ = false);
    StatsDistributionBinned(double min, double max, size_t num_bins, bool weighted_error_ = false);

    static StatsDistributionBinned *create(Estimator *estimator, bool weighted_error_ = false);
    
    virtual void addValue(double value);
    virtual void appendToFile(const std::string& name, std::ofstream& out);
    virtual std::string restoreFromFile(std::ifstream& in);

    // return the probability of the bin where value falls.
    double getProbability(double value);

    // finds the bin where value falls and returns its midpoint.
    double getBinnedValue(double value);

    double getValueAtIndex(size_t index);

    size_t getIndex(double value);

    class Iterator : StatsDistribution::Iterator {
      public:
        virtual double probability();
        virtual double probability(size_t pos);
        virtual double value();
        virtual void advance();
        virtual bool isDone();
        virtual void reset();
        virtual int position();
        virtual int totalCount();
        virtual double at(size_t pos);
        
      private:
        friend class StatsDistributionBinned;
        Iterator(StatsDistributionBinned *d);
        StatsDistributionBinned *distribution;

        int index;
    };
    
  protected:
    virtual StatsDistribution::Iterator *makeNewIterator();
  private:
    std::vector<double> breaks;  // size: number of bins + 1
    std::vector<double> mids;    // size: number of bins + 2 (left & right tail)
    std::vector<int> counts;     // size: number of bins + 2 (left & right tail)
    // the first and last elements in mids and counts
    //  will store the "tail," or counts for any values that fall outside
    //  of the precalculated bins.
    // the value of each will be the average of all values added to the tail.
    //  TODO: decide between mean and median for the tails.

    // total of bin weights = probability normalizer
    std::vector<double> bin_weights; //size: number of bins + 2 (left & right tail)
    double total_bin_weights; // computed and cached upon each sample addition

    // used until we have "enough" samples to pick bins.
    StatsDistributionAllSamples all_samples;

    std::multiset<double> all_samples_sorted;
    
    // TODO: set this in a principled way.
    static const size_t histogram_threshold = 50; // "enough" samples
    bool preset_breaks; // true iff the breaks were set explicitly via constructor

    void setBreaks(const std::vector<double>& new_breaks);

    void addToHistogram(double value);
    void addToTail(int index, double value);
    bool shouldRebin();
    void calculateBins();
    bool binsAreSet();

    void updateBin(int index, double value);
    double probabilityAtIndex(size_t index);

    void updateBinWeights(int new_sample_bin);

    std::string r_samples_name;
    void initRInside();

    RInside* R;

    void assertValidHistogram();
    void printHistogram();

    bool weighted_error;
};

#endif
