#ifndef SMALL_MAP_H_INCL
#define SMALL_MAP_H_INCL

#define SMALL_VECTOR_SMALL_MAP

#ifndef SMALL_VECTOR_SMALL_MAP

#include <tr1/unordered_map>
#define small_map std::tr1::unordered_map

#else

#include <vector>
#include <functional>

template <typename KeyType, typename ValueType>
class small_map {
    static const int DEFAULT_RESERVED_SPACE = 32;

  public:
    ValueType& operator[](const KeyType& key);
    void erase(KeyType& key);
    size_t size() const;
    size_t count(const KeyType& val) const;
    void clear();

    typedef std::pair<KeyType, ValueType> PairType;

    typedef typename std::vector<PairType >::const_iterator const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    typedef typename std::vector<PairType >::iterator iterator;
    iterator begin();
    iterator end();

    small_map() {
        items.reserve(DEFAULT_RESERVED_SPACE);
    }

  private:
    bool contains(const KeyType& key) const;
    
    std::vector<PairType> items;
};


template <typename KeyType, typename ValueType>
ValueType& 
small_map<KeyType, ValueType>::operator[](const KeyType& key)
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == key) {
            return items[i].second;
        }
    }
    items.push_back(std::make_pair(key, ValueType()));
    return items[items.size() - 1].second;
}

template <typename KeyType, typename ValueType>
void small_map<KeyType,ValueType>::erase(KeyType& key)
{
    for (typename std::vector<PairType>::iterator it = items.begin();
         it != items.end(); ++it) {
        if (it->first == key) {
            items.erase(it);
            return;
        }
    }
}

template <typename KeyType, typename ValueType>
size_t small_map<KeyType,ValueType>::size() const
{
    return items.size();
}

template <typename KeyType, typename ValueType>
size_t small_map<KeyType,ValueType>::count(const KeyType& key) const
{
    if (contains(key)) {
        return 1;
    } else {
        return 0;
    }
}

template <typename KeyType, typename ValueType>
void small_map<KeyType,ValueType>::clear()
{
    items.clear();
}

template <typename KeyType, typename ValueType>
bool small_map<KeyType,ValueType>::contains(const KeyType& key) const
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == key) {
            return true;
        }
    }
    return false;
}

template <typename KeyType, typename ValueType>
typename small_map<KeyType,ValueType>::const_iterator small_map<KeyType,ValueType>::begin() const
{
    return items.begin();
}

template <typename KeyType, typename ValueType>
typename small_map<KeyType,ValueType>::const_iterator small_map<KeyType,ValueType>::end() const
{
    return items.end();
}

template <typename KeyType, typename ValueType>
typename small_map<KeyType,ValueType>::iterator small_map<KeyType,ValueType>::begin()
{
    return items.begin();
}

template <typename KeyType, typename ValueType>
typename small_map<KeyType,ValueType>::iterator small_map<KeyType,ValueType>::end()
{
    return items.end();
}

#endif

#endif
