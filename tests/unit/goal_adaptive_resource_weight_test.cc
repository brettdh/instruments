#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "goal_adaptive_resource_weight_test.h"
#include "goal_adaptive_resource_weight.h"
#include "timeops.h"


CPPUNIT_TEST_SUITE_REGISTRATION(GoalAdaptiveResourceWeightTest);

void 
GoalAdaptiveResourceWeightTest::testWeight()
{
    fprintf(stderr, "Testing weight change over 1 second\n");

    weight = new GoalAdaptiveResourceWeight("test", 10, secondsInFuture(60));
    double initWeight = weight->getWeight();
    usleep(1000000);
    weight->reportSpentResource(0);
    
    weight->updateWeight();
    double smallerWeight = weight->getWeight();
    CPPUNIT_ASSERT(smallerWeight < initWeight);
    
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
    usleep(1000000);
    weight->reportSpentResource(0);
    weight->updateWeight();
    CPPUNIT_ASSERT(weight->getWeight() < prevWeight);
    prevWeight = weight->getWeight();
        
    for (int i = 0; i < (int) duration; ++i) {
        weight->reportSpentResource(duration - i + 1);
        usleep(1000000);
        
        //fprintf(stderr, "New weight: %.6f\n", weight->getWeight());
        double curWeight = weight->getWeight();
        struct timeval now;
        TIME(now);
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
        weight->reportSpentResource(0);
        weight->updateWeight();
        usleep((no_spend_seconds * 1000000) / num_updates);;
    }

    fprintf(stderr, "Waiting 60 seconds for expected weight update\n");
    double curWeight = weight->getWeight();
    weight->reportSpentResource(100);
        
    struct timeval end = secondsInFuture(60);
    struct timeval now;
    TIME(now);
    while (timercmp(&now, &end, <)) {
        if (weight->getWeight() - curWeight > 0.1) {
            return;
        }
        usleep(1000000);
        TIME(now);
    }
    CPPUNIT_FAIL("Weight should have been updated");
}

struct timeval
GoalAdaptiveResourceWeightTest::secondsInFuture(double seconds)
{
    struct timeval future;
    TIME(future);
    
    struct timeval addend = {(int)seconds, 0};
    addend.tv_usec = (suseconds_t) ((seconds - addend.tv_sec) * 1000000.0);
    
    timeradd(&future, &addend, &future);
    return future;
}

void GoalAdaptiveResourceWeightTest::setUp()
{
    weight = NULL;
}

void GoalAdaptiveResourceWeightTest::tearDown()
{
    delete weight;
    weight = NULL;
}
