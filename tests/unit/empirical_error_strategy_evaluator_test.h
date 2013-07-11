#ifndef empirical_error_strategy_evaluator_test_h_incl
#define empirical_error_strategy_evaluator_test_h_incl

#include <cppunit/TestFixture.h>
#include <cppunit/extensions/HelperMacros.h>

class Strategy;
class StrategyEvaluator;

class EmpiricalErrorStrategyEvaluatorTest : public CppUnit::TestFixture {

    CPPUNIT_TEST_SUITE(EmpiricalErrorStrategyEvaluatorTest);
    CPPUNIT_TEST(testSimpleExpectedValue);
    CPPUNIT_TEST(testMultipleEstimators);
    CPPUNIT_TEST(testMultipleEstimatorsTwice);
    CPPUNIT_TEST(testOnlyIterateOverRelevantEstimators);
    CPPUNIT_TEST(testSaveRestore);
    CPPUNIT_TEST(testEstimatorConditions);
    CPPUNIT_TEST_SUITE_END();

  public:
    void testSimpleExpectedValue();
    void testMultipleEstimators();
    void testMultipleEstimatorsTwice();
    void testOnlyIterateOverRelevantEstimators();
    void testSaveRestore();
    void testEstimatorConditions();

  private:
    void assertRestoredEvaluationMatches(Strategy **strategies, double *expected_values,
                                         size_t num_strategies, 
                                         StrategyEvaluator *evaluator, void **chooser_args);

    void assertValueBetweenErrors(const char *name, double value, 
                                  double low_error, double high_error);
};

#endif
