#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "goal_adaptive_resource_weight_test.h"
#include "goal_adaptive_resource_weight.h"
#include "timeops.h"

#include <stdlib.h>

#include "mocktime.h"

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(GoalAdaptiveResourceWeightTest, "slow");

void 
GoalAdaptiveResourceWeightTest::testWeight()
{
    fprintf(stderr, "Testing weight change over 1 second\n");

    weight = new GoalAdaptiveResourceWeight("test", 10, secondsInFuture(60));
    double initWeight = weight->getWeight();
    mocktime_usleep(1000000);
    weight->reportSpentResource(0);
    
    weight->updateWeight();
    double smallerWeight = weight->getWeight();
    CPPUNIT_ASSERT(smallerWeight < initWeight);

    mocktime_usleep(1000000);
    weight->reportSpentResource(5);
    double nextWeight = weight->getWeight();
    CPPUNIT_ASSERT_DOUBLES_EQUAL(smallerWeight, nextWeight, 0.00001);
    weight->updateWeight();
    double largerWeight = weight->getWeight();
    CPPUNIT_ASSERT(largerWeight > smallerWeight);
}

void 
GoalAdaptiveResourceWeightTest::testConstantlyIncreasingWeight()
{
    double duration = 20.0;
    fprintf(stderr, "Testing increasing weight over %.1f seconds\n", duration);

    struct timeval goalTime = secondsInFuture(duration);
    weight = new GoalAdaptiveResourceWeight("test", duration * (duration + 1) / 2.0, 
                                            goalTime);
    double initWeight = weight->getWeight();
    double prevWeight = initWeight;
    mocktime_usleep(1000000);
    weight->reportSpentResource(0);
    weight->updateWeight();
    CPPUNIT_ASSERT(weight->getWeight() < prevWeight);
    prevWeight = weight->getWeight();
        
    for (int i = 0; i < (int) duration; ++i) {
        mocktime_usleep(1000000);
        weight->reportSpentResource(duration - i + 1);
        weight->updateWeight();
        
        //fprintf(stderr, "New weight: %.6f\n", weight->getWeight());
        double curWeight = weight->getWeight();
        struct timeval now;
        mocktime_gettimeofday(&now, NULL);
        if (timercmp(&goalTime, &now, <)) {
            break;
        }
        CPPUNIT_ASSERT(prevWeight <= curWeight);
        prevWeight = weight->getWeight();
    }
    CPPUNIT_ASSERT(initWeight < prevWeight);
}

void 
GoalAdaptiveResourceWeightTest::testWeightUpdatesPeriodically()
{
    fprintf(stderr, "Testing periodic weight update\n");

    weight = new GoalAdaptiveResourceWeight("test", 100, secondsInFuture(60));

    // reduce the weight with zero-spending periods
    int no_spend_seconds = 5;
    int num_updates = 100;
    fprintf(stderr, "Spending nothing for %d seconds\n", no_spend_seconds);
    for (int i = 0; i < num_updates; ++i) {
        mocktime_usleep((no_spend_seconds * 1000000) / num_updates);
        weight->reportSpentResource(0);
        weight->updateWeight();
    }

    fprintf(stderr, "Waiting 60 seconds for expected weight update\n");
    double curWeight = weight->getWeight();
    mocktime_usleep(1000000);
    weight->reportSpentResource(100);
        
    struct timeval end = secondsInFuture(60);
    struct timeval now;
    mocktime_gettimeofday(&now, NULL);
    while (timercmp(&now, &end, <)) {
        if (weight->getWeight() - curWeight > 0.1) {
            return;
        }
        weight->updateWeight();
        
        mocktime_usleep(1000000);
        mocktime_gettimeofday(&now, NULL);
    }
    CPPUNIT_FAIL("Weight should have been updated");
}

struct timeval
GoalAdaptiveResourceWeightTest::secondsInFuture(double seconds)
{
    struct timeval future;
    mocktime_gettimeofday(&future, NULL);
    
    struct timeval addend = {(int)seconds, 0};
    addend.tv_usec = (suseconds_t) ((seconds - addend.tv_sec) * 1000000.0);
    
    timeradd(&future, &addend, &future);
    return future;
}

void GoalAdaptiveResourceWeightTest::setUp()
{
    weight = NULL;
    
    mocktime_enable_mocking();

    struct timeval now;
    gettimeofday(&now, NULL);
    mocktime_settimeofday(&now, NULL);
}

void GoalAdaptiveResourceWeightTest::tearDown()
{
    delete weight;
    weight = NULL;
}
