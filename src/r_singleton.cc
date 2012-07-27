#include "r_singleton.h"
#include <RInside.h>

RInside& get_rinside_instance()
{
    static RInside R;
    return R;
}
