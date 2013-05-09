#include "eval_method.h"

#include <assert.h>

#include <string>
#include <map>
using std::map;
using std::pair;
using std::string;

const int STATS_DISTRIBUTION_TYPE_MASK = 0xf;
const int JOINT_DISTRIBUTION_TYPE_MASK = 0xf0;


typedef map<EvalMethod, string> NameMap;
static NameMap::value_type names_initializer[] = {
    NameMap::value_type(TRUSTED_ORACLE, "trusted-oracle"),
    NameMap::value_type(CONFIDENCE_BOUNDS, "prob-bounds"),
    NameMap::value_type(BAYESIAN, "bayesian"),
    
    NameMap::value_type(EMPIRICAL_ERROR, "empirical-error"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES, "ee-all-samples"),
    NameMap::value_type(EMPIRICAL_ERROR_BINNED, "ee-binned"),
    NameMap::value_type(EMPIRICAL_ERROR_ALL_SAMPLES_INTNW, "ee-as-intnw"),
    NameMap::value_type(EMPIRICAL_ERROR_BINNED_INTNW, "ee-binned-intnw")
};

static NameMap names(names_initializer, 
                     names_initializer + sizeof(names_initializer) / sizeof(NameMap::value_type));

const char *
get_method_name(enum EvalMethod method)
{
    assert(names.count(method) == 1);
    return names[method].c_str();
}
