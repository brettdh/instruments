#ifndef STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL
#define STATS_DISTRIBUTION_ALL_SAMPLES_H_INCL

#include <deque>
#include "stats_distribution.h"

class StatsDistributionAllSamples : public StatsDistribution {
  public:
    StatsDistributionAllSamples(bool weighted_error_=false);
    virtual void addValue(double value);
    double getProbability(double value);
    virtual void appendToFile(const std::string& name, std::ofstream& out);
    virtual std::string restoreFromFile(std::ifstream& in);

    double probabilityAtPosition(size_t pos);

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
        friend class StatsDistributionAllSamples;
        Iterator(StatsDistributionAllSamples *d);
        StatsDistributionAllSamples *distribution;
        std::deque<double> cached_probability;
        size_t cur_position;
        size_t total_count;
    };
    
  protected:
    virtual StatsDistribution::Iterator *makeNewIterator();
  private:
    double getWeight(size_t sample_index);
    double calculateProbability(size_t sample_index);
    
    std::deque<double> values;
    
    // true iff distribution should use EWMA approach
    //  to de-emphasize older samples and emphasize newer samples
    bool weighted_error;
};

#endif
