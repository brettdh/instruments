#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "stats_distribution_test.h"
#include "stats_distribution.h"
#include "stats_distribution_all_samples.h"
#include "stats_distribution_binned.h"

#include <vector>
using std::vector;

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
            sanityCheckPDF(dist);
        }
    }

    delete dist;
}

void
StatsDistributionTest::testHistogramWithKnownBins()
{
    double b[] = {1.0,2.0,3.0,4.0,5.0};
    const size_t numbreaks = sizeof(b) / sizeof(double);
    vector<double> breaks(b, b + numbreaks);
    
    StatsDistribution *dist = new StatsDistributionBinned(breaks);
    for (size_t i = 0; i < numbreaks; ++i) {
        dist->addValue(b[i] + 0.5);
        sanityCheckPDF(dist);
    }
    dist->addValue(b[0] - 0.5);
    sanityCheckPDF(dist);

    const size_t count = numbreaks + 1;

    double expected_values[count] = {0.5, 1.5, 2.5, 3.5, 4.5, 5.5};

    StatsDistribution::Iterator *it;
    int i = 0;
    for (it = dist->getIterator(); !it->isDone(); it->advance(), ++i) {
        CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0/count, it->probability(), 0.001);
        CPPUNIT_ASSERT_DOUBLES_EQUAL(expected_values[i], it->value(), 0.001);
    }
    dist->finishIterator(it);

    delete dist;
}

void
StatsDistributionTest::sanityCheckPDF(StatsDistribution *dist)
{
    double pdf_sum = 0.0;
    StatsDistribution::Iterator *it;
    for (it = dist->getIterator(); !it->isDone(); it->advance()) {
        double prob = it->probability();
        CPPUNIT_ASSERT(prob < 1.0 || abs(prob - 1.0) < 0.001);
        pdf_sum += it->probability();
    }
    dist->finishIterator(it);
    
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, pdf_sum, 0.001);
}
