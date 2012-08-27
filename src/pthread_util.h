#ifndef pthread_util_incl
#define pthread_util_incl

#include <pthread.h>
#include <map>
#include <vector>
#include <deque>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "debug.h"

#define PTHREAD_ASSERT_SUCCESS(rc)                      \
    do {                                                \
        if (rc != 0) {                                  \
            fprintf(stderr, "PTHREAD ERROR: %s\n",      \
                    strerror(rc));                      \
            ASSERT(0);                                  \
        }                                               \
    } while (0)

#ifdef ANDROID
/* #include <boost/thread/shared_mutex.hpp> */
/* typedef boost::shared_mutex RWLOCK_T; */
/* #define RWLOCK_INIT(LOCK, ATTR) */
/* inline int RWLOCK_RDLOCK(RWLOCK_T *lock)   { lock->lock_shared();   return 0; } */
/* inline int RWLOCK_WRLOCK(RWLOCK_T *lock)   { lock->lock();          return 0; } */
/* inline int RWLOCK_RDUNLOCK(RWLOCK_T *lock) { lock->unlock_shared(); return 0; } */
/* inline int RWLOCK_WRUNLOCK(RWLOCK_T *lock) { lock->unlock();        return 0; } */
#else
typedef pthread_rwlock_t RWLOCK_T;
#define RWLOCK_INIT pthread_rwlock_init
#define RWLOCK_RDLOCK pthread_rwlock_rdlock
#define RWLOCK_WRLOCK pthread_rwlock_wrlock
#define RWLOCK_RDUNLOCK pthread_rwlock_unlock
#define RWLOCK_WRUNLOCK pthread_rwlock_unlock
#endif

class PthreadScopedLock {
  public:
    PthreadScopedLock() : mutex(NULL) {}

    explicit PthreadScopedLock(pthread_mutex_t *mutex_) {
        mutex = NULL;
        acquire(mutex_);
    }

    void acquire(pthread_mutex_t *mutex_) {
        ASSERT(!mutex);
        ASSERT(mutex_);
        mutex = mutex_;
        int rc = pthread_mutex_lock(mutex);
        PTHREAD_ASSERT_SUCCESS(rc);
    }
    
    ~PthreadScopedLock() {
        if (mutex) {
            release();
        }
    }
    void release() {
        int rc = pthread_mutex_unlock(mutex);
        PTHREAD_ASSERT_SUCCESS(rc);

        mutex = NULL;
    }
  private:
    pthread_mutex_t *mutex;
};

#if 0
class PthreadScopedRWLock {
  public:
    PthreadScopedRWLock() : mutex(NULL) {}

    explicit PthreadScopedRWLock(RWLOCK_T *mutex_, bool writer) {
        mutex = NULL;
        acquire(mutex_, writer);
    }

    void acquire(RWLOCK_T *mutex_, bool writer_) {
        ASSERT(!mutex);
        ASSERT(mutex_);
        mutex = mutex_;
        int rc = 0;
        writer = writer_;
        if (writer_) {
            rc = RWLOCK_WRLOCK(mutex);
        } else {
            rc = RWLOCK_RDLOCK(mutex);
        }
        PTHREAD_ASSERT_SUCCESS(rc);
    }

    ~PthreadScopedRWLock() {
        if (mutex) {
            release();
        }
    }
    void release() {
        int rc;
        if (writer) {
            rc = RWLOCK_WRUNLOCK(mutex);
        } else {
            rc = RWLOCK_RDUNLOCK(mutex);
        }
        PTHREAD_ASSERT_SUCCESS(rc);
        
        mutex = NULL;
    }
  private:
    RWLOCK_T *mutex;
    bool writer;
};

template <typename T>
class ThreadsafePrimitive {
  public:
    explicit ThreadsafePrimitive(const T& v = T()) : val(v) {
        RWLOCK_INIT(&lock, NULL);
    }
    operator T() {
        PthreadScopedRWLock lk(&lock, false);
        return val;
    }
    ThreadsafePrimitive<T>& operator=(const T& v) {
        PthreadScopedRWLock lk(&lock, true);
        val = v;
        return *this;
    }
  private:
    T val;
    RWLOCK_T lock;
};

template <typename T>
class LockWrappedQueue {
  public:
    LockWrappedQueue() {
        pthread_mutex_init(&lock, NULL);
        pthread_cond_init(&cv, NULL);
    }
    size_t size() {
        PthreadScopedLock lk(&lock);
        return q.size();
    }
    bool empty() { return size() == 0; }
    void push(const T& val) {
        PthreadScopedLock lk(&lock);
        q.push_back(val);
        pthread_cond_broadcast(&cv);
    }
    void pop(T& val) {
        PthreadScopedLock lk(&lock);
        while (q.empty()) {
            pthread_cond_wait(&cv, &lock);
        }
        val = q.front();
        q.pop_front();
    }

    // iteration is not thread-safe.
    typedef typename std::deque<T>::iterator iterator;
    iterator begin() {
        return q.begin();
    }

    iterator end() {
        return q.end();
    }
  private:
    std::deque<T> q;
    pthread_mutex_t lock;
    pthread_cond_t cv;
};

/* LockWrappedMap
 *
 * Simple wrapper around an STL map that makes
 * the basic operations (insert, find, erase)
 * thread-safe.  Useful if you want any per-object
 * locks to live inside the contained objects.
 */
template <typename KeyType, typename ValueType,
          typename ordering = std::less<KeyType> >
class LockWrappedMap {
    typedef std::map<KeyType, ValueType, ordering> MapType;

  public:
    LockWrappedMap() {
        pthread_mutex_init(&membership_lock, NULL);
    }

    ~LockWrappedMap() {
        pthread_mutex_destroy(&membership_lock);
    }

    typedef typename MapType::iterator iterator;

    bool insert(const KeyType& key, const ValueType& val) {
        PthreadScopedLock lock(&membership_lock);
        std::pair<iterator, bool> ret = 
            the_map.insert(std::make_pair(key, val));
        return ret.second;
    }

    bool find(const KeyType &key, ValueType& val) {
        PthreadScopedLock lock(&membership_lock);
        typename MapType::iterator pos = the_map.find(key);
        if (pos != the_map.end()) {
            val = pos->second;
            return true;
        } else {
            return false;
        }
    }

    bool erase(const KeyType &key) {
        PthreadScopedLock lock(&membership_lock);
        return (the_map.erase(key) == 1);
    }

    /* Not thread-safe, obviously. */
    iterator begin() {
        return the_map.begin();
    }

    /* Not thread-safe, obviously. */
    iterator end() {
        return the_map.end();
    }

  private:
    MapType the_map;
    pthread_mutex_t membership_lock;
};
#endif

#endif
