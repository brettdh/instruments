#ifndef STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL
#define STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL

#include <vector>
#include "stats_distribution.h"

class StatsDistributionAllSamples : public StatsDistribution {
  public:
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
        friend class StatsDistributionAllSamples;
        Iterator(StatsDistributionAllSamples *d);
        StatsDistributionAllSamples *distribution;
        double cached_probability;
        int cur_position;
        int total_count;
    };
    
  protected:
    virtual StatsDistribution::Iterator *makeNewIterator();
  private:
    std::vector<double> values;
};

#endif
