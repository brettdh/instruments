#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "stats_distribution_test.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "stats_distribution_binned.h"

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

    sanityCheckPDF(dist);
    
    StatsDistribution::Iterator *it;
    for (it = dist->getIterator(); !it->isDone(); it->advance()) {
        CPPUNIT_ASSERT_DOUBLES_EQUAL(value, it->value(), 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_prob, it->probability(), 0.001);
    }
    dist->finishIterator(it);
}

void
StatsDistributionTest::testHistogram()
{
    StatsDistribution *dist = new StatsDistributionBinned;

    int counts[] = {1,2,4,8,16,32,16,8,4,2,1};
    const int numints = sizeof(counts) / sizeof(int);
    for (int i = 0; i < numints; ++i) {
        for (int j = 0; j < counts[i]; ++j) {
            dist->addValue(i);
        }
    }

    sanityCheckPDF(dist);
}

void
StatsDistributionTest::sanityCheckPDF(StatsDistribution *dist)
{
    double pdf_sum = 0.0;
    StatsDistribution::Iterator *it;
    for (it = dist->getIterator(); !it->isDone(); it->advance()) {
        CPPUNIT_ASSERT(it->probability() < 1.0);
        pdf_sum += it->probability();
    }
    dist->finishIterator(it);
    
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, pdf_sum, 0.001);
}
