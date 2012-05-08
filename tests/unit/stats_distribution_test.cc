#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "stats_distribution_test.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"

CPPUNIT_TEST_SUITE_REGISTRATION(StatsDistributionTest);

void
StatsDistributionTest::testIdenticalSamples()
{
    double value = 3.0;
    int reps = 5;
    StatsDistribution *dist = new StatsDistributionAllSamples;
    for (int i = 0; i < reps; ++i) {
        dist->addValue(value);
    }
    double expected_prob = 1.0 / reps;

    StatsDistribution::Iterator *it;
    for (it = dist->getIterator(); !it->isDone(); it->advance()) {
        CPPUNIT_ASSERT_DOUBLES_EQUAL(value, it->value(), 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_prob, it->probability(), 0.001);
    }
    dist->finishIterator(it);
}
