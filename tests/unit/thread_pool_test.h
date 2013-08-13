#ifndef THREAD_POOL_TEST_H_INCL_0YAGHREG0Q
#define THREAD_POOL_TEST_H_INCL_0YAGHREG0Q

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class ThreadPoolTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(ThreadPoolTest);
    CPPUNIT_TEST(testAsynchrony);
    CPPUNIT_TEST(testThreadCount);
    CPPUNIT_TEST(testTaskScheduling);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testAsynchrony();
    void testThreadCount();
    void testTaskScheduling();
};

#endif
