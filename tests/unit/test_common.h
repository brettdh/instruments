#ifndef TEST_COMMON_H_INCLUDED_OUHAU9EYGRAHR
#define TEST_COMMON_H_INCLUDED_OUHAU9EYGRAHR


#include <cppunit/SourceLine.h>

void assertEqWithin(const std::string& actual_str, 
                    const std::string& expected_str, 
                    const std::string& message, 
                    double expected, double actual, double alpha,
                    CppUnit::SourceLine line);

#define MY_CPPUNIT_ASSERT_EQWITHIN_MESSAGE(expected,actual,alpha, message) \
  assertEqWithin(#actual, #expected,                                    \
                 message, expected, actual, alpha, CPPUNIT_SOURCELINE())

#endif
