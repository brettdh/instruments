#ifndef running_mean_estimator_test_h_incl
#define running_mean_estimator_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class RunningMeanEstimatorTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(RunningMeanEstimatorTest);
    CPPUNIT_TEST(testSimple);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testSimple();
};

#endif
