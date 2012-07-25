#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include <Rcpp.h>
using Rcpp::Environment; using Rcpp::Function;
using Rcpp:as; using Rcpp:wrap; using Rcpp::SEXP;

#include <string>
#include <vector>

CPPUNIT_TEST_SUITE_REGISTRATION(RTest);

void
RTest::testHistogram()
{
    Environment histogram_package("package:histogram");
    Function histogram = histogram_package["histogram"];
    
    const int count = 10;
    std::vector<int> numbers;
    for (int i = 0; i < COUNT; ++i) {
        numbers.push_back(i);
    }
    
    SEXP hist_result_R = histogram(wrap(numbers));
    std::map<std::string, SEXP> hist_result = as(hist_result_R);
    CPPUNIT_ASSERT_MESSAGE("Result from R has data",
                           !hist_result.empty());
    
    Language attr_fn("attr", wrap(hist_result), wrap("class"));
    std::string classname = as(attr_fn.eval());
    CPPUNIT_ASSERT_EQUAL(std::string("histogram"), classname);

    const size_t NUM_KEYS = 7;
    const char *keys[NUM_KEYS] = {"breaks", "counts", "intensities", 
                                  "density", "mids", "xname", "equidist"};
    for (size_t i = 0; i < NUM_KEYS; ++i) {
        std::string key(keys[i]);
        CPPUNIT_ASSERT_MESSAGE(std::string("histogram contains key: ") + key,
                               hist_result.count(key) > 0);
    }
}
