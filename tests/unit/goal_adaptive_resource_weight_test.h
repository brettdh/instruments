#ifndef GOAL_ADAPTIVE_RESOURCE_WEIGHT_TEST_H_INCL
#define GOAL_ADAPTIVE_RESOURCE_WEIGHT_TEST_H_INCL

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>
#include <sys/time.h>

class GoalAdaptiveResourceWeight;

class GoalAdaptiveResourceWeightTest : public CppUnit::TestFixture {
    CPPUNIT_TEST_SUITE(GoalAdaptiveResourceWeightTest);
    CPPUNIT_TEST(testWeight);
    CPPUNIT_TEST(testConstantlyIncreasingWeight);
    CPPUNIT_TEST(testWeightUpdatesPeriodically);
    CPPUNIT_TEST_SUITE_END();

  public:
    void setUp();
    void tearDown();

    void testWeight();
    void testConstantlyIncreasingWeight();
    void testWeightUpdatesPeriodically();
    
  private:
    GoalAdaptiveResourceWeight *weight;
    struct timeval secondsInFuture(double seconds);
};

#endif
