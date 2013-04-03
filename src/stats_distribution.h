#ifndef STATS_DISTRIBUTION
#define STATS_DISTRIBUTION

#include "iterator.h"
#include "small_set.h"

#include <string>
#include <fstream>

class StatsDistribution {
  public:
    virtual ~StatsDistribution() {}
    virtual void addValue(double value) = 0;

    class Iterator : public ::Iterator {
        //
    };
    Iterator *getIterator();
    void finishIterator(Iterator *it);

    virtual void appendToFile(const std::string& name, std::ofstream& out) = 0;
    virtual std::string restoreFromFile(std::ifstream& in) = 0;

  protected:
    virtual Iterator *makeNewIterator() = 0;
  private:
    small_set<Iterator *> iterators;
};

#endif
