#include "multi_dimension_array.h"
#include <vector>
using std::vector;

#define BOOST_DISABLE_ASSERTS
#include <boost/multi_array.hpp>

#include <blitz/array.h>

#include <stdio.h>
#include <sys/time.h>

int main()
{
    size_t size = 4;
    const size_t rowlen = 30;
    vector<size_t> dims;
    dims.resize(size, rowlen);

    vector<size_t> position;
    position.resize(size, 0);
    
    double ****raw_array = new double***[rowlen];
    boost::multi_array<double, 4> boost_array(boost::extents[rowlen][rowlen][rowlen][rowlen]);
    blitz::Array<double, 4> blitz_array(rowlen, rowlen, rowlen, rowlen);
    blitz::TinyVector<size_t, 4> index;
    for (size_t i = 0; i < rowlen; ++i) {
        raw_array[i] = new double**[rowlen];
        index[0] = i;
        for (size_t j = 0; j < rowlen; ++j) {
            raw_array[i][j] = new double*[rowlen];
            index[1] = j;
            for (size_t k = 0; k < rowlen; ++k) {
                raw_array[i][j][k] = new double[rowlen];
                index[2] = k;
                for (size_t m = 0; m < rowlen; ++m) {
                    raw_array[i][j][k][m] = 42.0;
                    boost_array[i][j][k][m] = 42.0;

                    index[3] = m;
                    blitz_array(index) = 42.0;
                }
            }
        }
    }
    MultiDimensionArray<double> array(dims, 42.0);

    // ===== RAW ARRAY ===== //
    
    struct timeval begin, end, diff;
    gettimeofday(&begin, NULL);
    double sum = 0.0;
    for (size_t i = 0; i < rowlen; ++i) {
        for (size_t j = 0; j < rowlen; ++j) {
            for (size_t k = 0; k < rowlen; ++k) {
                for (size_t m = 0; m < rowlen; ++m) {
                    sum += raw_array[i][j][k][m];
                }
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

    // ===== MultiDimensionArray ===== //

    gettimeofday(&begin, NULL);
    sum = 0.0;
    for (size_t i = 0; i < rowlen; ++i) {
        position[0] = i;
        for (size_t j = 0; j < rowlen; ++j) {
            position[1] = j;
            for (size_t k = 0; k < rowlen; ++k) {
                position[2] = k;
                for (size_t m = 0; m < rowlen; ++m) {
                    position[3] = m;
                    sum += array.at(position);
                }
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
    for (size_t i = 0; i < rowlen; ++i) {
        for (size_t j = 0; j < rowlen; ++j) {
            for (size_t k = 0; k < rowlen; ++k) {
                for (size_t m = 0; m < rowlen; ++m) {
                    sum += boost_array[i][j][k][m];
                }
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
    for (size_t i = 0; i < rowlen; ++i) {
        index[0] = i;
        for (size_t j = 0; j < rowlen; ++j) {
            index[1] = j;
            for (size_t k = 0; k < rowlen; ++k) {
                index[2] = k;
                for (size_t m = 0; m < rowlen; ++m) {
                    index[3] = m;
                    sum += blitz_array(index);
                }
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
