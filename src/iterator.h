#ifndef ITERATOR_H_INCL
#define ITERATOR_H_INCL

#include <sys/types.h>

class Iterator {
  public:
    virtual double probability() = 0;
    virtual double probability(size_t pos) = 0;
    virtual double value() = 0;
    virtual void advance() = 0;
    virtual bool isDone() = 0;
    virtual void reset() = 0;
    virtual int position() = 0;
    virtual int totalCount() = 0;
    virtual double at(size_t pos) = 0;
    virtual ~Iterator() {}
};

#endif
