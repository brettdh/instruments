#ifndef stats_distribution_test_h_incl
#define stats_distribution_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class StatsDistribution;

class StatsDistributionTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(StatsDistributionTest);
    CPPUNIT_TEST(testIdenticalSamples);
    CPPUNIT_TEST(testHistogram);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testIdenticalSamples();
    void testHistogram();

  private:
    void sanityCheckPDF(StatsDistribution *dist);
};

#endif
