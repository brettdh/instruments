#ifndef strategy_estimators_discovery_test_h_incl
#define strategy_estimators_discovery_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class StrategyEstimatorsDiscoveryTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(StrategyEstimatorsDiscoveryTest);
    CPPUNIT_TEST(testEstimatorsDiscoveredAtRegistration);

    // Not running this test for now, since the use case that
    //  it tests isn't present in my applications, and catching
    //  this case incurs a performance cost in the common case.
    //CPPUNIT_TEST(testEstimatorsDiscoveredUponLaterUse);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testEstimatorsDiscoveredAtRegistration();
    void testEstimatorsDiscoveredUponLaterUse();
};

#endif
