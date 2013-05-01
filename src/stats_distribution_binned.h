#ifndef STATS_DISTRIBUTION_BINNED_H_INCL
#define STATS_DISTRIBUTION_BINNED_H_INCL

#include <vector>
#include <set>
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"

#include <RInside.h>

class StatsDistributionBinned : public StatsDistribution {
  public:
    StatsDistributionBinned();
    StatsDistributionBinned(std::vector<double> breaks);
    StatsDistributionBinned(double min, double max, size_t num_bins);
    
    virtual void addValue(double value);
    virtual void appendToFile(const std::string& name, std::ofstream& out);
    virtual std::string restoreFromFile(std::ifstream& in);

    // return the probability of the bin where value falls.
    double getProbability(double value);

    class Iterator : StatsDistribution::Iterator {
      public:
        virtual double probability();
        virtual double probability(int pos);
        virtual double value();
        virtual void advance();
        virtual bool isDone();
        virtual void reset();
        virtual int position();
        virtual int totalCount();
        virtual double at(int pos);
        
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

    // used until we have "enough" samples to pick bins.
    StatsDistributionAllSamples all_samples;

    std::multiset<double> all_samples_sorted;
    
    // TODO: set this in a principled way.
    static const size_t histogram_threshold = 50; // "enough" samples
    bool preset_breaks; // true iff the breaks were set explicitly via constructor

    void setBreaks(const std::vector<double>& new_breaks);

    void addToHistogram(double value);
    void addToTail(int& count, double& mid, double value);
    bool shouldRebin();
    void calculateBins();
    bool binsAreSet();

    size_t getIndex(double value);
    void updateBin(int index, double value);
    double probabilityAtIndex(size_t index);


    std::string r_samples_name;
    void initRInside();

    RInside* R;

    void assertValidHistogram();
    void printHistogram();
};

#endif
