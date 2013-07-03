#include "eval_method.h"
#include "debug.h"
namespace inst = instruments;
using inst::ERROR;

#include <assert.h>
#include <stdlib.h>

#include <string>
#include <map>
#include <set>
using std::map;
using std::set;
using std::pair;
using std::string;

const int STATS_DISTRIBUTION_TYPE_MASK = 0xf;
const int JOINT_DISTRIBUTION_TYPE_MASK = 0xf0;


typedef map<EvalMethod, string> NameMap;
static NameMap::value_type names_initializer[] = {
    NameMap::value_type(TRUSTED_ORACLE, "trusted-oracle"),
    NameMap::value_type(CONFIDENCE_BOUNDS, "prob-bounds"),
    NameMap::value_type(CONFIDENCE_BOUNDS_WEIGHTED, "prob-bounds-weighted"),
    NameMap::value_type(BAYESIAN, "bayesian"),
    NameMap::value_type(BAYESIAN, "bayesian-weighted"),
    
    NameMap::value_type(EMPIRICAL_ERROR, "empirical-error"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES, "ee-all-samples"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED, "ee-as-weighted"),
    NameMap::value_type(EMPIRICAL_ERROR_BINNED, "ee-binned"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES_INTNW, "ee-as-intnw"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES_WEIGHTED_INTNW, "ee-as-weighted-intnw"),
    NameMap::value_type(EMPIRICAL_ERROR_BINNED_INTNW, "ee-binned-intnw")
};

static NameMap names(names_initializer, 
                     names_initializer + sizeof(names_initializer) / sizeof(NameMap::value_type));

const char *
get_method_name(enum EvalMethod method)
{
    ASSERT(names.count(method) == 1);
    return names[method].c_str();
}

EvalMethod
get_method(const char *method_name)
{
    for (auto& p : names) {
        const string& name = p.second;
        if (name == method_name) {
            return p.first;
        }
    }
    inst::dbgprintf(ERROR, "Invalid method name: %s\n", method_name);
    abort();
}

set<string> get_all_method_names()
{
    set<string> method_names;
    for (auto& p : names) {
        method_names.insert(p.second);
    }
    return method_names;
}
