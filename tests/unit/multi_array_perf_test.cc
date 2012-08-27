#include "multi_dimension_array.h"
#include <vector>
using std::vector;

#include <stdio.h>
#include <sys/time.h>

#define BOOST_DISABLE_ASSERTS
#include <boost/multi_array.hpp>

#include <blitz/array.h>

int main()
{
    const size_t iterations = 30*30;

    size_t size = 2;
    const size_t rowlen = 30;
    vector<size_t> dims;
    dims.resize(size, rowlen);

    vector<size_t> position;
    position.resize(size, 0);
    
    double **raw_array = new double*[rowlen];
    double *flat_array = new double[rowlen * rowlen];
    boost::multi_array<double, 2> boost_array(boost::extents[rowlen][rowlen]);
    blitz::Array<double, 2> blitz_array(rowlen, rowlen);
    blitz::TinyVector<size_t, 2> index;
    for (size_t i = 0; i < rowlen; ++i) {
        raw_array[i] = new double[rowlen];
        index[0] = i;
        for (size_t j = 0; j < rowlen; ++j) {
            raw_array[i][j] = 42.0;
            flat_array[i * rowlen + j] = 42.0;
            boost_array[i][j] = 42.0;

            index[1] = j;
            blitz_array(index) = 42.0;
        }
    }
    MultiDimensionArray<double> array(dims, 42.0);

    // ===== RAW ARRAY ===== //
    
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    double sum = 0.0;
    for (size_t z = 0; z < iterations; ++z) {
        for (size_t i = 0; i < rowlen; ++i) {
            for (size_t j = 0; j < rowlen; ++j) {
                sum += raw_array[i][j];
            }
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

    fprintf(stderr, "raw array took %f seconds; sum=%f\n", seconds, sum);

    // ===== FLAT ARRAY ===== //

    gettimeofday(&begin, NULL);
    sum = 0.0;
    for (size_t z = 0; z < iterations; ++z) {
        for (size_t i = 0; i < rowlen; ++i) {
            for (size_t j = 0; j < rowlen; ++j) {
                sum += flat_array[i * rowlen + j];
            }
        }
    }
    gettimeofday(&end, NULL);
    diff.tv_sec = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) {
        --diff.tv_sec;
        end.tv_usec += 1000000;
    }
    diff.tv_usec = end.tv_usec - begin.tv_usec;
    seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);

    fprintf(stderr, "flat array took %f seconds; sum=%f\n", seconds, sum);

    // ===== MultiDimensionArray ===== //

    gettimeofday(&begin, NULL);
    sum = 0.0;
    for (size_t z = 0; z < iterations; ++z) {
        for (size_t i = 0; i < rowlen; ++i) {
            position[0] = i;
            for (size_t j = 0; j < rowlen; ++j) {
                position[1] = j;
                sum += array.at(position);
            }
        }
    }
    gettimeofday(&end, NULL);
    diff.tv_sec = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) {
        --diff.tv_sec;
        end.tv_usec += 1000000;
    }
    diff.tv_usec = end.tv_usec - begin.tv_usec;
    seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);

    fprintf(stderr, "MultiDimensionArray took %f seconds; sum=%f\n", seconds, sum);

    
    // ===== boost::multi_array ===== //

    gettimeofday(&begin, NULL);
    sum = 0.0;
    for (size_t z = 0; z < iterations; ++z) {
        for (size_t i = 0; i < rowlen; ++i) {
            for (size_t j = 0; j < rowlen; ++j) {
                sum += boost_array[i][j];
            }
        }
    }
    gettimeofday(&end, NULL);
    diff.tv_sec = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) {
        --diff.tv_sec;
        end.tv_usec += 1000000;
    }
    diff.tv_usec = end.tv_usec - begin.tv_usec;
    seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);

    fprintf(stderr, "boost::multi_array took %f seconds; sum=%f\n", seconds, sum);

    // ===== blitz::Array ===== //

    gettimeofday(&begin, NULL);
    sum = 0.0;
    for (size_t z = 0; z < iterations; ++z) {
        for (size_t i = 0; i < rowlen; ++i) {
            index[0] = i;
            for (size_t j = 0; j < rowlen; ++j) {
                index[1] = j;
                sum += blitz_array(index);
            }
        }
    }
    gettimeofday(&end, NULL);
    diff.tv_sec = end.tv_sec - begin.tv_sec;
    if (end.tv_usec < begin.tv_usec) {
        --diff.tv_sec;
        end.tv_usec += 1000000;
    }
    diff.tv_usec = end.tv_usec - begin.tv_usec;
    seconds = diff.tv_sec + (diff.tv_usec / 1000000.0);

    fprintf(stderr, "blitz::Array took %f seconds; sum=%f\n", seconds, sum);
    
    return 0;
}
