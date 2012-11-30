#ifndef SMALL_MAP_H_INCL
#define SMALL_MAP_H_INCL

#define SMALL_VECTOR_SMALL_MAP

#ifndef SMALL_VECTOR_SMALL_MAP

#include <tr1/unordered_map>
#define small_map std::tr1::unordered_map

#else

#include <vector>
#include <functional>

template <typename KeyType, typename MappedType>
class small_map {
    static const int DEFAULT_RESERVED_SPACE = 32;

  public:
    // so that they look like the STL equivalents
    typedef KeyType key_type;
    typedef MappedType mapped_type;
    typedef std::pair<key_type, mapped_type> value_type;

    MappedType& operator[](const KeyType& key);
    void erase(KeyType& key);
    size_t size() const;
    size_t count(const KeyType& val) const;
    void clear();
    
    typedef typename std::vector<value_type>::const_iterator const_iterator;
    const_iterator begin() const;
    const_iterator end() const;

    typedef typename std::vector<value_type>::iterator iterator;
    iterator begin();
    iterator end();

    small_map() {
        items.reserve(DEFAULT_RESERVED_SPACE);
    }

  private:
    bool contains(const KeyType& key) const;
    
    std::vector<value_type> items;
};


template <typename KeyType, typename MappedType>
MappedType& 
small_map<KeyType, MappedType>::operator[](const KeyType& key)
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == key) {
            return items[i].second;
        }
    }
    items.push_back(std::make_pair(key, MappedType()));
    return items[items.size() - 1].second;
}

template <typename KeyType, typename MappedType>
void small_map<KeyType,MappedType>::erase(KeyType& key)
{
    for (typename std::vector<value_type>::iterator it = items.begin();
         it != items.end(); ++it) {
        if (it->first == key) {
            items.erase(it);
            return;
        }
    }
}

template <typename KeyType, typename MappedType>
size_t small_map<KeyType,MappedType>::size() const
{
    return items.size();
}

template <typename KeyType, typename MappedType>
size_t small_map<KeyType,MappedType>::count(const KeyType& key) const
{
    if (contains(key)) {
        return 1;
    } else {
        return 0;
    }
}

template <typename KeyType, typename MappedType>
void small_map<KeyType,MappedType>::clear()
{
    items.clear();
}

template <typename KeyType, typename MappedType>
bool small_map<KeyType,MappedType>::contains(const KeyType& key) const
{
    for (size_t i = 0; i < items.size(); ++i) {
        if (items[i].first == key) {
            return true;
        }
    }
    return false;
}

template <typename KeyType, typename MappedType>
typename small_map<KeyType,MappedType>::const_iterator small_map<KeyType,MappedType>::begin() const
{
    return items.begin();
}

template <typename KeyType, typename MappedType>
typename small_map<KeyType,MappedType>::const_iterator small_map<KeyType,MappedType>::end() const
{
    return items.end();
}

template <typename KeyType, typename MappedType>
typename small_map<KeyType,MappedType>::iterator small_map<KeyType,MappedType>::begin()
{
    return items.begin();
}

template <typename KeyType, typename MappedType>
typename small_map<KeyType,MappedType>::iterator small_map<KeyType,MappedType>::end()
{
    return items.end();
}

#endif

#endif
