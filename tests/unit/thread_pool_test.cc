#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "thread_pool_test.h"
#include "thread_pool.h"

#include <sys/time.h>
#include <unistd.h>

#include <thread>
#include <set>
using std::mutex; using std::lock_guard;
using std::set;

CPPUNIT_TEST_SUITE_REGISTRATION(ThreadPoolTest);

void
ThreadPoolTest::testAsynchrony()
{
    ThreadPool *pool = new ThreadPool(3);
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    auto task = []() {
        usleep(100000); // 100 ms
    };
    for (int i = 0; i < 3; ++i) {
        pool->startTask(task);
    }
    gettimeofday(&end, NULL);
    timersub(&end, &begin, &diff);
    CPPUNIT_ASSERT_MESSAGE("starting tasks took less than a second",
                           diff.tv_sec == 0);
    CPPUNIT_ASSERT_MESSAGE("starting tasks took less than 50 ms",
                           diff.tv_usec < 50000);
    
    gettimeofday(&begin, NULL);
    delete pool; // joins to all workers, which finish their last tasks first
    gettimeofday(&end, NULL);
    timersub(&end, &begin, &diff);
    CPPUNIT_ASSERT_MESSAGE("finishing tasks took less than a second",
                           diff.tv_sec == 0);
    CPPUNIT_ASSERT_MESSAGE("finishing tasks took more than 100 ms",
                           diff.tv_usec >= 100000);

    // they should sleep in parallel
    CPPUNIT_ASSERT_MESSAGE("finishing tasks took less than 200 ms",
                           diff.tv_usec < 200000);
}

void
ThreadPoolTest::testThreadCount()
{
    mutex lock;
    
    set<pthread_t> threads;
    
    auto task = [&]() {
        lock.lock();
        threads.insert(pthread_self());
        lock.unlock();
        
        usleep(50000);
    };
    
    const int num_threads = 3;
    ThreadPool *pool = new ThreadPool(num_threads);
    for (int i = 0; i < 3 * num_threads; ++i) {
        pool->startTask(task);
    }
    delete pool;
    
    CPPUNIT_ASSERT_EQUAL(num_threads, (int)threads.size());
}
