#ifndef STATS_DISTRIBUTION
#define STATS_DISTRIBUTION

#include "iterator.h"
#include "small_set.h"

class StatsDistribution {
  public:
    virtual void addValue(double value) = 0;

    class Iterator : public ::Iterator {
        //
    };
    Iterator *getIterator();
    void finishIterator(Iterator *it);

  protected:
    virtual Iterator *makeNewIterator() = 0;
  private:
    small_set<Iterator *> iterators;
};

#endif
