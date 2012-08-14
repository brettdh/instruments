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
    
    // for testing only
    StatsDistributionBinned(std::vector<double> breaks);

    virtual void addValue(double value);

    class Iterator : StatsDistribution::Iterator {
      public:
        virtual double probability();
        virtual double value();
        virtual void advance();
        virtual bool isDone();
        virtual void reset();
        virtual int position();
        virtual int totalCount();
        
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

    void addToHistogram(double value);
    void addToTail(int& count, double& mid, double value);
    bool shouldRebin();
    void calculateBins();
    bool binsAreSet();

    void updateBin(int index, double value);

    std::string r_samples_name;
    void initRInside();

    RInside* R;

    void assertValidHistogram();
    void printHistogram();
};

#endif
