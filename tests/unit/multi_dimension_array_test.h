#ifndef multi_dimension_array_test_h_incl
#define multi_dimension_array_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

#include <vector>

template <typename T> class MultiDimensionArray;

class MultiDimensionArrayTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(MultiDimensionArrayTest);
    CPPUNIT_TEST(testInitialize);
    CPPUNIT_TEST(testValueSetting);
    CPPUNIT_TEST_SUITE_END();

  public:
    void setUp();
    void tearDown();

    void testInitialize();
    void testValueSetting();
  private:
    MultiDimensionArray<double> *array;
    std::vector<size_t> dimensions;
};

#endif
