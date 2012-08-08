#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "multi_dimension_array_test.h"
#include "multi_dimension_array.h"

#include <vector>
using std::vector;

CPPUNIT_TEST_SUITE_REGISTRATION(MultiDimensionArrayTest);

void
MultiDimensionArrayTest::setUp()
{
    dimensions.clear();
    dimensions.push_back(3);
    dimensions.push_back(3);
    dimensions.push_back(3);
    
    array = new MultiDimensionArray<double>(dimensions, -1.0);
}

void
MultiDimensionArrayTest::tearDown()
{
    delete array;
}

void MultiDimensionArrayTest::testInitialize()
{
    vector<size_t> indices;
    indices.push_back(0);
    indices.push_back(0);
    indices.push_back(0);
    for (size_t i = 0; i < 3; ++i) {
        indices[0] = i;
        for (size_t j = 0; j < 3; ++j) {
            indices[1] = j;
            for (size_t k = 0; k < 3; ++k) {
                indices[2] = k;
                double value = array->at(indices);
                CPPUNIT_ASSERT_DOUBLES_EQUAL(-1.0, value, 0.001);
            }
        }
    }
}

void MultiDimensionArrayTest::testValueSetting()
{
    vector<size_t> indices;
    indices.push_back(1);
    indices.push_back(0);
    indices.push_back(2);
    array->at(indices) = 42.0;
    CPPUNIT_ASSERT_DOUBLES_EQUAL(42.0, array->at(indices), 0.001);
}
