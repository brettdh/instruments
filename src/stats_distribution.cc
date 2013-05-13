#include "stats_distribution.h"

StatsDistribution::Iterator *
StatsDistribution::getIterator()
{
    Iterator *it = makeNewIterator();
    iterators.insert(it);
    return it;
}

void 
StatsDistribution::finishIterator(Iterator *it)
{
    iterators.erase(it);
    delete it;
}

int
StatsDistribution::totalCount()
{
    int total;
    Iterator *it = getIterator();
    total = it->totalCount();
    finishIterator(it);
    return total;
}
