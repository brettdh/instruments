#ifndef SMALL_SET_H_INCL
#define SMALL_SET_H_INCL

#define SMALL_VECTOR_SMALL_SET

#ifndef SMALL_VECTOR_SMALL_SET

#include <tr1/unordered_set>
#define small_set std::tr1::unordered_set

#else

#include <vector>

template <typename T>
class small_set {
    static const int DEFAULT_RESERVED_SPACE = 32;
  public:
    void insert(T& val);
    void erase(T& val);
    size_t size() const;
    size_t count(const T& val) const;
    void clear();

    typedef typename std::vector<T>::const_iterator const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    small_set() {
        items.reserve(DEFAULT_RESERVED_SPACE);
    }

    template <typename InputIterator>
    small_set(InputIterator first, InputIterator last)
        : items(first, last) {}
    
  private:
    bool contains(const T& item) const;
    
    std::vector<T> items;
};

template <typename T>
void small_set<T>::insert(T& val)
{
    if (!contains(val)) {
        items.push_back(val);
    }
}

template <typename T>
void small_set<T>::erase(T& val)
{
    for (typename std::vector<T>::iterator it = items.begin();
         it != items.end(); ++it) {
        if (*it == val) {
            items.erase(it);
            return;
        }
    }
}

template <typename T>
size_t small_set<T>::size() const
{
    return items.size();
}

template <typename T>
size_t small_set<T>::count(const T& val) const
{
    if (contains(val)) {
        return 1;
    } else {
        return 0;
    }
}

template <typename T>
void small_set<T>::clear()
{
    items.clear();
}

template <typename T>
bool small_set<T>::contains(const T& val) const
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i] == val) {
            return true;
        }
    }
    return false;
}

template <typename T>
typename small_set<T>::const_iterator small_set<T>::begin() const
{
    return items.begin();
}

template <typename T>
typename small_set<T>::const_iterator small_set<T>::end() const
{
    return items.end();
}


#endif

#endif
