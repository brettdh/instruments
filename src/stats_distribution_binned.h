#ifndef STATS_DISTRIBUTION_BINNED_H_INCL
#define STATS_DISTRIBUTION_BINNED_H_INCL

#include <vector>
#include "stats_distribution.h"

class StatsDistributionBinned : public StatsDistribution {
  public:
    virtual void addValue(double value);

    class Iterator : StatsDistribution::Iterator {
      public:
        virtual double probability();
        virtual double value();
        virtual void advance();
        virtual bool isDone();
        
      private:
        friend class StatsDistributionBinned;
        Iterator(StatsDistributionBinned *d);
        StatsDistributionBinned *distribution;
        std::vector<double>::const_iterator real_iterator;
    };
    
  protected:
    virtual StatsDistribution::Iterator *makeNewIterator();
  private:
    // TODO: finish implementing.
};

#endif
