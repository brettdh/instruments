#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "thread_pool_test.h"
#include "thread_pool.h"

#include <sys/time.h>
#include <unistd.h>

#include <thread>
#include <chrono>
#include <set>
#include <vector>
#include <functional>
using std::mutex; using std::unique_lock; using std::condition_variable;
using std::set; using std::vector; using std::bind;
namespace this_thread = std::this_thread;
using std::chrono::milliseconds;

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

void
ThreadPoolTest::testTaskScheduling()
{
    mutex lock;
    condition_variable cv;
    vector<int> finished;
    
    auto task_fn = [&](int create_order, int run_order) {
        unique_lock<mutex> guard(lock);
        fprintf(stderr, "Task %d done (create order: %d)\n", run_order, create_order);
        finished.push_back(run_order);
        cv.notify_one();
    };

    ThreadPool *pool = new ThreadPool(3);
    pool->scheduleTask(0.25, bind(task_fn, 0, 1));
    pool->scheduleTask(0.50, bind(task_fn, 1, 3));
    this_thread::sleep_for(milliseconds(100));
    pool->scheduleTask(0.01, bind(task_fn, 2, 0));
    pool->scheduleTask(0.30, bind(task_fn, 3, 2));
    pool->scheduleTask(1.0, bind(task_fn, 4, 4));

    size_t num_tasks = 5;
    
    ThreadPool::TimerTaskPtr task = pool->scheduleTask(0.50, bind(task_fn, 5, 42));
    this_thread::sleep_for(milliseconds(200));
    
    task->cancel();
    unique_lock<mutex> guard(lock);
    while (finished.size() < num_tasks) {
        cv.wait(guard);
    }
    int last_id = -1;
    for (int id : finished) {
        CPPUNIT_ASSERT_MESSAGE("Tasks ran in expected order", last_id < id);
        last_id = id;
    }
    delete pool;
}
