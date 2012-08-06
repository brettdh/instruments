#ifndef STATS_DISTRIBUTION
#define STATS_DISTRIBUTION

#include "small_set.h"

class StatsDistribution {
  public:
    virtual void addValue(double value) = 0;

    class Iterator {
      public:
        virtual double probability() = 0;
        virtual double value() = 0;
        virtual void advance() = 0;
        virtual bool isDone() = 0;
        virtual void reset() = 0;
        virtual ~Iterator() {}
    };
    
    Iterator *getIterator();
    void finishIterator(Iterator *it);

  protected:
    virtual Iterator *makeNewIterator() = 0;
  private:
    small_set<Iterator *> iterators;
};

#endif
