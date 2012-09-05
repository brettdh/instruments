#ifndef MULTI_DIMENSION_ARRAY_H_INCL
#define MULTI_DIMENSION_ARRAY_H_INCL

#include <vector>
#include <assert.h>
#include <sys/types.h>

template <typename T>
class MultiDimensionArray {
  public:
    MultiDimensionArray<T>(const std::vector<size_t>& dimensions, const T& inital_value);
    ~MultiDimensionArray();
    
    const T& at(const std::vector<size_t>& indices) const;
    T& at(const std::vector<size_t>& indices);
    
  private:
    void *the_array;
    std::vector<size_t> dimensions;
    size_t num_dimensions;
    
    void *initializeArray(const T& initial_value, size_t next=0);
    void destroyArray(void *array, size_t next=0);
    T& at(void *array, const std::vector<size_t>& indices, size_t next);
};

template <typename T>
MultiDimensionArray<T>::MultiDimensionArray(const std::vector<size_t>& dimensions_, 
                                            const T& initial_value)
{
    assert(dimensions_.size() > 0);
    dimensions = dimensions_;
    num_dimensions = dimensions.size();
    the_array = initializeArray(initial_value);
}

template <typename T>
MultiDimensionArray<T>::~MultiDimensionArray()
{
    destroyArray(the_array);
}

template <typename T>
void *
MultiDimensionArray<T>::initializeArray(const T& initial_value, size_t next)
{
    if (next + 1 == num_dimensions) {
        T *values = new T[dimensions[next]];
        for (size_t i = 0; i < dimensions[next]; ++i) {
            values[i] = initial_value;
        }
        return values;
    }
    void **bridges = new void *[dimensions[next]];
    for (size_t i = 0; i < dimensions[next]; ++i) {
        bridges[i] = initializeArray(initial_value, next + 1);
    }
    return bridges;
}

template <typename T>
void 
MultiDimensionArray<T>::destroyArray(void *array, size_t next)
{
    if (next + 1 == num_dimensions) {
        T *valueArray = (T*) array;
        delete [] valueArray;
        return;
    }
    void **bridges = (void **)array;
    for (size_t i = 0; i < dimensions[next]; ++i) {
        destroyArray(bridges[i], next + 1);
    }
    delete [] bridges;
}

template <typename T>
const T& MultiDimensionArray<T>::at(const std::vector<size_t>& indices) const
{
    return at(the_array, indices, 0);
}

template <typename T>
T& MultiDimensionArray<T>::at(const std::vector<size_t>& indices)
{
    return at(the_array, indices, 0);
}

template <typename T>
T& MultiDimensionArray<T>::at(void *array, const std::vector<size_t>& indices, size_t next)
{
    assert(indices.size() == num_dimensions);
    assert(indices[next] >= 0);
    assert(indices[next] < dimensions[next]);
    
//#define RECURSIVE
#ifdef RECURSIVE
    if (next + 1 == num_dimensions) {
        T *valuesArray = (T*) array;
        return valuesArray[indices[next]];
    }
    void **bridges = (void **)array;
    return at(bridges[indices[next]], indices, next + 1);
#else
    while (next + 1 < num_dimensions) {
        void **bridges = (void **)array;
        array = bridges[indices[next]];
        ++next;
    }
    T *valuesArray = (T*) array;
    return valuesArray[indices[next]];
#endif
}

#endif
