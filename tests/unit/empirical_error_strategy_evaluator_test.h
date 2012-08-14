#ifndef empirical_error_strategy_evaluator_test_h_incl
#define empirical_error_strategy_evaluator_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class EmpiricalErrorStrategyEvaluatorTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(EmpiricalErrorStrategyEvaluatorTest);
    CPPUNIT_TEST(testSimpleExpectedValue);
    CPPUNIT_TEST(testMultipleEstimators);
    CPPUNIT_TEST(testMultipleEstimatorsTwice);
    CPPUNIT_TEST(testOnlyIterateOverRelevantEstimators);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testSimpleExpectedValue();
    void testMultipleEstimators();
    void testMultipleEstimatorsTwice();
    void testOnlyIterateOverRelevantEstimators();
};

#endif
