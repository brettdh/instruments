#include "multi_dimension_array.h"
#include <vector>
using std::vector;

#include <stdio.h>
#include <sys/time.h>

int main()
{
    size_t size = 2;
    size_t rowlen = 5000;
    vector<size_t> dims;
    dims.resize(size, rowlen);

    vector<size_t> position;
    position.resize(size, 0);
    
    MultiDimensionArray<double> array(dims, 42.0);

    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    double sum = 0.0;
    for (size_t i = 0; i < rowlen; ++i) {
        position[0] = i;
        for (size_t j = 0; j < rowlen; ++j) {
            position[1] = j;
            sum += array.at(position);
        }
    }
    gettimeofday(&end, NULL);
    diff.tv_sec = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) {
        --diff.tv_sec;
        end.tv_usec += 1000000;
    }
    diff.tv_usec = end.tv_usec - begin.tv_usec;
    double seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);

    fprintf(stderr, "took %f seconds; sum=%f\n", seconds, sum);
    return 0;
}
