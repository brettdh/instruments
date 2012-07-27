#include <cppunit/Test.h>
#include <cppunit/TestAssert.h>
#include <cppunit/extensions/HelperMacros.h>

#include <RInside.h>
using Rcpp::Environment; using Rcpp::Function;
using Rcpp::List; using Rcpp::Language;
using Rcpp::as; using Rcpp::wrap;

#include "r_singleton.h"

#include <string>
#include <vector>
#include <sstream>

#include "r_test.h"
#include "timeops.h"

CPPUNIT_TEST_SUITE_REGISTRATION(RTest);

static void mark_timepoint(const char *msg)
{
    static std::string last_msg;
    static struct timeval last_timestamp = {0, 0};

    struct timeval now;
    TIME(now);
    
    if (last_msg.length() > 0) {
        struct timeval diff;
        TIMEDIFF(last_timestamp, now, diff);
        fprintf(stderr, "%s took %lu.%06lu seconds\n",
                last_msg.c_str(), diff.tv_sec, diff.tv_usec);
    }
    if (msg) {
        last_msg.assign(msg);
    } else {
        last_msg.assign("");
    }
    last_timestamp = now;
}

void
RTest::testHistogram()
{
    mark_timepoint("Creating RInside instance");
    RInside& R = get_rinside_instance();
    mark_timepoint("loading histogram library");
    R.parseEvalQ("library(histogram)");
    mark_timepoint(NULL);
    
    const int COUNT = 10;
    std::vector<int> numbers;
    for (int i = 0; i < COUNT; ++i) {
        numbers.push_back(i);
    }

    mark_timepoint("assigning vector to R object");
    R["numbers"] = numbers;
    mark_timepoint("running histogram function");
    const char *str = "histogram::histogram(numbers, verbose=FALSE, plot=FALSE)";
    Rcpp::List hist_result = R.parseEval(str);
    mark_timepoint(NULL);
    
    Language attr_fn("attr", wrap(hist_result), wrap("class"));
    std::string classname = as<std::string>(attr_fn.eval());
    CPPUNIT_ASSERT_EQUAL(std::string("histogram"), classname);

    const size_t NUM_KEYS = 7;
    const char *keys[NUM_KEYS] = {"breaks", "counts", "intensities", 
                                  "density", "mids", "xname", "equidist"};
    for (size_t i = 0; i < NUM_KEYS; ++i) {
        std::string key(keys[i]);
        CPPUNIT_ASSERT_MESSAGE(std::string("histogram contains key: ") + key,
                               hist_result[key] != (SEXP) NULL);
    }

    std::vector<double> breaks;
    breaks.push_back(0);
    breaks.push_back(9);
    assertListMatches("breaks matches", breaks, hist_result["breaks"]);
    
    std::vector<double> counts;
    counts.push_back(10);
    assertListMatches("counts matches", counts, hist_result["counts"]);

    std::vector<double> density;
    density.push_back(1.0/9);
    assertListMatches("density matches", density, hist_result["density"]);

    assertListMatches("intensities matches", density, hist_result["intensities"]);

    std::vector<double> mids;
    mids.push_back(4.5);
    assertListMatches("mids matches", mids, hist_result["mids"]);

    CPPUNIT_ASSERT_EQUAL(true, as<bool>(hist_result["equidist"]));

    CPPUNIT_ASSERT_EQUAL(std::string("numbers"), 
                         as<std::string>(hist_result["xname"]));
}

void
RTest::assertListMatches(const std::string& msg, 
                         std::vector<double> expected, SEXP actual)
{
    std::vector<double> actual_vector = as<std::vector<double> >(actual);
    CPPUNIT_ASSERT_EQUAL_MESSAGE(msg + std::string(": same size"),
                                 expected.size(), actual_vector.size());
    for (size_t i = 0; i < expected.size(); ++i) {
        std::ostringstream oss;
        oss << msg << ": item " << i;
        CPPUNIT_ASSERT_DOUBLES_EQUAL_MESSAGE(oss.str(),
                                             expected[i], actual_vector[i], 0.001);
    }
}
