#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include "running_mean_estimator_test.h"
#include "running_mean_estimator.h"

CPPUNIT_TEST_SUITE_REGISTRATION(RunningMeanEstimatorTest);

void
RunningMeanEstimatorTest::testSimple()
{
    Estimator *estimator = Estimator::create(RUNNING_MEAN, "simple");
    estimator->addObservation(1.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(1.0, estimator->getEstimate(), 0.001);
    estimator->addObservation(0.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, estimator->getEstimate(), 0.001);
    estimator->addObservation(1.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.6666, estimator->getEstimate(), 0.001);
    estimator->addObservation(0.0);
    CPPUNIT_ASSERT_DOUBLES_EQUAL(0.5, estimator->getEstimate(), 0.001);
}
