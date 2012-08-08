#ifndef STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL
#define STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL

#include <deque>
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
        std::deque<double>::const_iterator real_iterator;
        double cached_probability;
        int cur_position;
    };
    
  protected:
    virtual StatsDistribution::Iterator *makeNewIterator();
  private:
    std::deque<double> values;
};

#endif
