#ifndef r_test_h_incl
#define r_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <vector>
#include <string>
#include <RInside.h>

class RTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(RTest);
    CPPUNIT_TEST(testHistogram);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testHistogram();

  private:
    void assertListMatches(const std::string& msg, 
                           std::vector<double> clist, SEXP rlist);
};

#endif
