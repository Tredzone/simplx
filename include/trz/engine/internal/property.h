/**
 * @file property.h
 * @brief graph property class
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cstring>
#include <list>
#include <ostream>
#include <typeinfo>

#include "trz/engine/internal/mforwardchain.h"

#include "trz/engine/platform.h"

namespace tredzone
{
/**
 * @brief Output operator for std::list
 * @param os output-stream
 * @param value const reference to a list
 * @return os
 */
template <class T> std::ostream &operator<<(std::ostream &os, const std::list<T> &value)
{
    typename std::list<T>::const_iterator iterator;
    os << "{" << std::boolalpha;
    for (iterator = value.begin(); iterator != value.end(); ++iterator)
    {
        os << std::boolalpha << *iterator << ",";
    }
    os << "}" << std::boolalpha;
    return os;
}
template <class _Allocator> class Property;

/**
 * @brief Non-template base class to Property template.
 */
class PropertyBase
{
};

class PropertyNullValue;

/**
 * @brief Non-template polymorphic (virtual) base class to PropertyNullValue and PropertyValue template.
 */
class PropertyAbstractValue
{
  public:
    /**
     * @brief Destructor.
     */
    virtual ~PropertyAbstractValue() noexcept {}
    /**
     * @brief Appends string representation to provided output-stream.
     * @param os output-stream
     * @return os
     */
    virtual std::ostream &toOStream(std::ostream &) const = 0;
    /**
     * @brief Comparator.
     * @param nullValue null-value
     * @return true if this property-value is a null-value.
     */
    inline bool operator==(const PropertyNullValue &nullValue) const noexcept;
    /**
     * @brief Comparator.
     * @param nullValue null-value
     * @return true if this property-value is not a null-value.
     */
    inline bool operator!=(const PropertyNullValue &nullValue) const noexcept;

  private:
    template <class> friend class Property;
    virtual PropertyAbstractValue *clone(const PropertyBase &) const = 0; // throw (std::bad_alloc)
};

/**
 * @brief null-value singleton.
 */
class PropertyNullValue : public PropertyAbstractValue
{
  public:
    /**
     * @brief Getter.
     * @return singleton instance.
     */
    inline static PropertyNullValue &getInstance() noexcept
    {
        static PropertyNullValue instance;
        return instance;
    }
    /**
     * @see PropertyAbstractValue::toOStream().
     */
    virtual std::ostream &toOStream(std::ostream &os) const { return os << "null"; }
    /**
     * @brief Comparator.
     * @param other another property-value.
     * @return true if other is a null-value.
     */
    inline bool operator==(const PropertyAbstractValue &other) const noexcept { return this == &other; }
    /**
     * @brief Comparator.
     * @param other another property-value.
     * @return true if other is not a null-value.
     */
    inline bool operator!=(const PropertyAbstractValue &other) const noexcept { return this != &other; }

  private:
    inline PropertyNullValue() noexcept {}
    /**
     * throw (std::bad_alloc)
     */
    virtual PropertyAbstractValue *clone(const PropertyBase &) const { return 0; }
    PropertyNullValue(const PropertyNullValue &);
};

/**
 * @brief Generic property-value wrapper.
 */
template <class _Allocator, class _Value> class PropertyValue : public PropertyAbstractValue
{
  private:
    const Property<_Allocator> &property;

  public:
    _Value value; ///< Wrapped value.

    /**
     * @brief Default constructor.
     * @param property property container.
     */
    inline PropertyValue(const Property<_Allocator> &pproperty) : property(pproperty) {}
    /**
     * @brief Constructor with default value initialization.
     * @param property property container.
     * @param valueInit initial value.
     */
    template <class _ValueInit>
    inline PropertyValue(const Property<_Allocator> &pproperty, const _ValueInit &valueInit)
        : property(pproperty), value(valueInit)
    {
    }
    /**
     * @brief Destructor.
     */
    virtual ~PropertyValue() noexcept {}
    /**
     * @see PropertyAbstractValue::toOStream().
     */
    virtual std::ostream &toOStream(std::ostream &os) const { return os << std::boolalpha << value; }

  private:
    friend class Property<_Allocator>;
    virtual PropertyAbstractValue *clone(const PropertyBase &) const; // throw (std::bad_alloc)
    void *operator new(size_t);                                       // throw (std::bad_alloc)
    inline void *operator new(std::size_t size, void *ptr) noexcept { return ::operator new(size, ptr); }
    inline void operator delete(void *)noexcept;
    inline void operator delete(void *, void *)noexcept;
};

/**
 * @brief std::string property-value specialization.
 */
template <class _Allocator>
class PropertyValue<_Allocator,
                    std::basic_string<char, std::char_traits<char>, typename _Allocator::template rebind<char>::other>>
    : public PropertyAbstractValue
{
  public:
    typedef std::basic_string<char, std::char_traits<char>,
                              typename _Allocator::template rebind<char>::other>
        string_type;   ///< Alias to std::string using generic template type _Allocator.
    string_type value; ///< Wrapped string-value.

    /**
     * @brief Default constructor.
     * @param property property container.
     */
    inline PropertyValue(const Property<_Allocator> &property) : value(property.get_allocator()) {}
    /**
     * @brief Constructor with default value initialization.
     * @param property property container.
     * @param valueInit initial string-value.
     */
    inline PropertyValue(const Property<_Allocator> &property, const char *init) : value(init, property.get_allocator())
    {
    }
    /**
     * @brief Constructor with default value initialization.
     * @param property property container.
     * @param valueInit initial string-value.
     */
    inline PropertyValue(const Property<_Allocator> &property, const string_type &init)
        : value(init.c_str(), property.get_allocator())
    {
    }
    /**
     * @brief Destructor.
     */
    virtual ~PropertyValue() noexcept {}
    /**
     * @see PropertyAbstractValue::toOStream().
     */
    virtual std::ostream &toOStream(std::ostream &os) const { return os << '\"' << value << '\"'; }

  private:
    friend class Property<_Allocator>;
    virtual PropertyAbstractValue *clone(const PropertyBase &) const; // throw (std::bad_alloc)
    void *operator new(size_t);                                       // throw (std::bad_alloc)
    inline void *operator new(std::size_t size, void *ptr) noexcept { return ::operator new(size, ptr); }
    inline void operator delete(void *)noexcept;
    inline void operator delete(void *, void *)noexcept;
};

/**
 * @brief key-value-property container.
 * key is a string.
 * value is a public sub-class of PropertyAbstractValue.
 */
template <class _Allocator> class Property : public PropertyBase
{
  public:
    typedef _Allocator allocator_type;
    typedef std::basic_string<char, std::char_traits<char>,
                              typename _Allocator::template rebind<char>::other>
        string_type; ///< Alias to std::string using generic template type _Allocator.
    typedef PropertyAbstractValue AbstractValue;
    class Collection;

    /**
     * @brief Default constructor.
     * @param allocator STL-compliant allocator.
     * @throw std::bad_alloc
     */
    inline Property(const allocator_type &pAllocator = allocator_type());
    /**
     * @brief Copy constructor.
     * @param other another key-value-property.
     * @throw std::bad_alloc
     */
    inline Property(const Property &other);
    /**
     * @brief Destructor.
     */
    inline ~Property() noexcept;
    /**
     * @brief Assignment operator.
     * @param other another key-value-property.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    inline Property &operator=(const Property &other);
    /**
     * @brief Getter.
     * @return key.
     */
    inline const char *getName() const noexcept;
    /**
     * @brief Getter.
     * @return value.
     * @throw std::bad_cast
     */
    template <class _Value> inline _Value &getValue();
    /**
     * @brief Getter.
     * @return const value.
     * @throw std::bad_cast
     */
    template <class _Value> inline const _Value &getValue() const;
    /**
     * @brief Getter.
     * @return property-value wrapper.
     */
    inline AbstractValue &getAbstractValue() noexcept;
    /**
     * @brief Getter.
     * @return const property-value wrapper.
     */
    inline const AbstractValue &getAbstractValue() const noexcept;
    /**
     * @brief Setter.
     * @param key
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _NameStringType> inline Property &setName(const _NameStringType &);
    /**
     * @brief Setter.
     * Sets default value of template generic type _Value.
     * _Value must publicly inherit from PropertyAbstractValue.
     * @attention Standard output operator must have a specialization compatible with _Value.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _Value> inline Property &setValue();
    /**
     * @brief Setter.
     * Sets default value of template generic type _Value.
     * _Value must publicly inherit from PropertyAbstractValue.
     * @attention Standard output operator must have a specialization compatible with _Value.
     * @param valueInit initial value.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _Value, class _ValueInit> inline Property &setValue(const _ValueInit &);
    /**
     * @brief Setter.
     * Sets value vith a copy of the provided property-value.
     * @param value initial value.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    inline Property &setAbstractValue(const AbstractValue &);
    /**
     * @brief Setter.
     * Sets default value of template generic type _Value.
     * _Value must publicly inherit from PropertyAbstractValue.
     * @attention Standard output operator must have a specialization compatible with _Value.
     * @param key
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _Value, class _NameStringType> inline Property &setNameAndValue(const _NameStringType &);
    /**
     * @brief Setter.
     * Sets default value of template generic type _Value.
     * _Value must publicly inherit from PropertyAbstractValue.
     * @attention Standard output operator must have a specialization compatible with _Value.
     * @param key
     * @param valueInit initial value.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _Value, class _ValueInit, class _NameStringType>
    inline Property &setNameAndValue(const _NameStringType &, const _ValueInit &);
    /**
     * @brief Setter.
     * Sets value vith a copy of the provided property-value.
     * @param key
     * @param value initial value.
     * @return This key-value-property.
     * @throw std::bad_alloc
     */
    template <class _NameStringType>
    inline Property &setNameAndAbstractValue(const _NameStringType &, const AbstractValue &);
    /**
     * @brief Getter.
     * @return null-value.
     */
    inline static const PropertyNullValue &nullValue() noexcept;
    /**
     * @brief Getter.
     * @return An allocator instance.
     */
    inline allocator_type get_allocator() const noexcept;
    /**
     * @brief Appends string representation to provided output-stream.
     * @param os output-stream
     * @return os
     */
    inline std::ostream &toOStream(std::ostream &) const;

  private:
    allocator_type allocator;
    char *name;
    AbstractValue *value;

    inline char *newName(const char *); // throw (std::bad_alloc)
    template <class _NameStringAllocator>
    inline char *
    newName(const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &); // throw (std::bad_alloc)
};

/**
 * @brief Tree like container of key-value-properties.
 */
template <class _Allocator>
class Property<_Allocator>::Collection : private MultiForwardChainLink<Collection>, public Property<_Allocator>
{
  private:
    struct Chain : Collection::template ForwardChain<0u, Chain>
    {
        inline static Collection *getItem(MultiForwardChainLink<Collection> *link) noexcept
        {
            return static_cast<Collection *>(link);
        }
        inline static const Collection *getItem(const MultiForwardChainLink<Collection> *link) noexcept
        {
            return static_cast<const Collection *>(link);
        }
        inline static MultiForwardChainLink<Collection> *getLink(Collection *item) noexcept
        {
            return static_cast<MultiForwardChainLink<Collection> *>(item);
        }
        inline static const MultiForwardChainLink<Collection> *getLink(const Collection *item) noexcept
        {
            return static_cast<const MultiForwardChainLink<Collection> *>(item);
        }
    };

  public:
    typedef typename Property<_Allocator>::allocator_type allocator_type;
    typedef size_t size_type;
    typedef typename Chain::iterator iterator;             ///< Alias to stl-compliant iterator.
    typedef typename Chain::const_iterator const_iterator; ///< Alias to stl-compliant const iterator.

    /**
     * @brief Default constructor.
     * @param allocator
     * @throw std::bad_alloc
     */
    inline Collection(const allocator_type &pAllocator = allocator_type());
    /**
     * @brief Constructor.
     * @param other another key-value-property.
     * @throw std::bad_alloc
     */
    inline Collection(const Property<_Allocator> &);
    /**
     * @brief Copy constructor.
     * @param other another property-container.
     * @throw std::bad_alloc
     */
    inline Collection(const Collection &);
    /**
     * @brief Destructor.
     */
    inline ~Collection() noexcept;
    /**
     * @brief Assignment operator.
     * @param other another key-value-property.
     * @return This property-container.
     * @throw std::bad_alloc
     */
    inline Collection &operator=(const Property<_Allocator> &);
    /**
     * @brief Assignment operator.
     * @param other another property-container.
     * @return This property-container.
     * @throw std::bad_alloc
     */
    inline Collection &operator=(const Collection &);
    /**
     * @brief Getter.
     * @return First key-value-property iterator.
     */
    inline iterator begin() noexcept;
    /**
     * @brief Getter.
     * @return First key-value-property const iterator.
     */
    inline const_iterator begin() const noexcept;
    /**
     * @brief Getter.
     * @return Past-end key-value-property iterator.
     */
    inline iterator end() noexcept;
    /**
     * @brief Getter.
     * @return Past-end key-value-property const iterator.
     */
    inline const_iterator end() const noexcept;
    /**
     * @brief Getter.
     * @return true if this property-container is empty.
     */
    inline bool empty() const noexcept;
    /**
     * @brief Getter.
     * Searches for first key-value-property matching the provided key.
     * @param key
     * @return iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    template <class _NameStringType> inline iterator find(const _NameStringType &) noexcept;
    /**
     * @brief Getter.
     * Starting from the provided iterator,
     * searches for first key-value-property matching the provided key.
     * @param from starting iterator-position search point.
     * @param key
     * @return starting from the provided iterator-position,
     * iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    inline iterator find(iterator, const char *) noexcept;
    /**
     * @brief Getter.
     * Starting from the provided iterator,
     * searches for first key-value-property matching the provided key.
     * @param from starting iterator-position search point.
     * @param key
     * @return starting from the provided iterator-position,
     * iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    template <class _NameStringAllocator>
    inline iterator find(iterator,
                         const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &) noexcept;
    /**
     * @brief Getter.
     * Searches for first key-value-property matching the provided key.
     * @param key
     * @return iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    template <class _NameStringType> inline const_iterator find(const _NameStringType &) const noexcept;
    /**
     * @brief Getter.
     * Starting from the provided iterator,
     * searches for first key-value-property matching the provided key.
     * @param from starting iterator-position search point.
     * @param key
     * @return starting from the provided iterator-position,
     * iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    inline const_iterator find(const_iterator, const char *) const noexcept;
    /**
     * @brief Getter.
     * Starting from the provided iterator,
     * searches for first key-value-property matching the provided key.
     * @param from starting iterator-position search point.
     * @param key
     * @return starting from the provided iterator-position,
     * iterator to first entry matching provided key.
     * If no entry found, returns past-end iterator.
     */
    template <class _NameStringAllocator>
    inline const_iterator find(const_iterator,
                               const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &) const
        noexcept;
    /**
     * @brief Inserts an entry at the containers end.
     * @note The entry is created with an empty string key and a null-value.
     * @return The iterator-position to the inserted entry.
     * @throw std::bad_alloc
     */
    inline iterator insert();
    /**
     * @brief Inserts an entry at the provided iterator-position.
     * @note The entry is created with an empty string key and a null-value.
     * @param i iterator-position of the entry to be inserted.
     * @return The iterator-position to the inserted entry.
     * @throw std::bad_alloc
     */
    inline iterator insert(iterator);
    /**
     * @brief Searches for first entry using provided key.
     * If not found, inserts at the end (see insert()) a new (provided key, null-value) entry.
     * @param key
     * @return The iterator-position to the found or inserted entry.
     * @throw std::bad_alloc
     */
    template <class _NameStringType> inline iterator update(const _NameStringType &);
    /**
     * @brief Starting provided iterator-position, searches for first entry using provided key.
     * If not found, inserts at the end (see insert()) a new (provided key, null-value) entry.
     * @param from starting iterator-position search point.
     * @param key
     * @return The iterator-position to the found or inserted entry.
     * @throw std::bad_alloc
     */
    template <class _NameStringType> inline iterator update(iterator, const _NameStringType &);
    /**
     * @brief Searches for first entry using provided key.
     * If not found, inserts at the end (see insert()) a new (provided key, template generic type _Value()) entry.
     * @param key
     * @return The pair (pointer to value, iterator-position to the found or inserted entry).
     * @throw std::bad_alloc
     */
    template <class _Value, class _NameStringType>
    inline std::pair<_Value *, iterator> updateValue(const _NameStringType &);
    /**
     * @brief Starting provided iterator-position, Searches for first entry using provided key.
     * If not found, inserts at the end (see insert()) a new (provided key, template generic type _Value()) entry.
     * @param from starting iterator-position search point.
     * @param key
     * @return The pair (pointer to value, iterator-position to the found or inserted entry).
     * @throw std::bad_alloc
     */
    template <class _Value, class _NameStringType>
    inline std::pair<_Value *, iterator> updateValue(iterator, const _NameStringType &);
    /**
     * @brief Removes an entry.
     * @attention The provided iterator-position must be valid.
     * @param i iterator-position of the entry to be removed.
     * @return The next iterator-position to the erased entry.
     */
    inline iterator erase(iterator) noexcept;
    /**
     * @brief Empties this property-container.
     */
    inline void clear() noexcept;
    /**
     * @brief Appends string representation to provided output-stream.
     * @param os output-stream
     * @return os
     */
    inline std::ostream &toOStream(std::ostream &, bool = true) const;

  private:
    Chain leaves;
};

bool PropertyAbstractValue::operator==(const PropertyNullValue &nullValue) const noexcept { return this == &nullValue; }

bool PropertyAbstractValue::operator!=(const PropertyNullValue &nullValue) const noexcept { return this != &nullValue; }

template <class _Allocator, class _Value>
PropertyAbstractValue *PropertyValue<_Allocator, _Value>::clone(const PropertyBase &potherProperty) const
{
    const Property<_Allocator> &otherProperty = static_cast<const Property<_Allocator> &>(potherProperty);
    return new (typename _Allocator::template rebind<PropertyValue>::other(otherProperty.get_allocator()).allocate(1))
        PropertyValue(otherProperty, value);
}

template <class _Allocator, class _Value> void PropertyValue<_Allocator, _Value>::operator delete(void *p) noexcept
{
    typename _Allocator::template rebind<PropertyValue>::other(((PropertyValue *)p)->property.get_allocator())
        .deallocate((PropertyValue *)p, 1);
}

template <class _Allocator, class _Value>
void PropertyValue<_Allocator, _Value>::operator delete(void *, void *p) noexcept
{
    typename _Allocator::template rebind<PropertyValue>::other(((PropertyValue *)p)->property.get_allocator())
        .deallocate((PropertyValue *)p, 1);
}

template <class _Allocator>
PropertyAbstractValue *
PropertyValue<_Allocator,
              std::basic_string<char, std::char_traits<char>, typename _Allocator::template rebind<char>::other>>::
    clone(const PropertyBase &potherProperty) const
{
    const Property<_Allocator> &otherProperty = static_cast<const Property<_Allocator> &>(potherProperty);
    return new (typename _Allocator::template rebind<PropertyValue>::other(otherProperty.get_allocator()).allocate(1))
        PropertyValue(otherProperty, value);
}

template <class _Allocator>
void PropertyValue<_Allocator,
                   std::basic_string<char, std::char_traits<char>, typename _Allocator::template rebind<char>::other>>::
operator delete(void *p) noexcept
{
    typename _Allocator::template rebind<PropertyValue>::other(((PropertyValue *)p)->value.get_allocator())
        .deallocate((PropertyValue *)p, 1);
}

template <class _Allocator>
void PropertyValue<_Allocator,
                   std::basic_string<char, std::char_traits<char>, typename _Allocator::template rebind<char>::other>>::
operator delete(void *, void *p) noexcept
{
    typename _Allocator::template rebind<PropertyValue>::other(((PropertyValue *)p)->value.get_allocator())
        .deallocate((PropertyValue *)p, 1);
}

template <class _Allocator>
Property<_Allocator>::Property(const allocator_type &pallocator) : allocator(pallocator), name(0), value(0)
{
}

template <class _Allocator>
Property<_Allocator>::Property(const Property &other)
    : allocator(other.allocator), name(newName(other.name)), value(other.getAbstractValue().clone(*this))
{
}

template <class _Allocator> Property<_Allocator>::~Property() noexcept
{
    if (name != 0)
    {
        typename allocator_type::template rebind<char>::other(allocator).deallocate(name, std::strlen(name) + 1);
    }
    if (value != 0)
    {
        delete value;
    }
}

template <class _Allocator> char *Property<_Allocator>::newName(const char *pName)
{
    if (pName != 0 && *pName != '\0')
    {
        size_t sz = std::strlen(pName) + 1;
        char *newName = typename allocator_type::template rebind<char>::other(allocator).allocate(sz);
        std::memcpy(newName, pName, sz);
        return newName;
    }
    else
    {
        return 0;
    }
}

template <class _Allocator>
template <class _NameStringAllocator>
char *Property<_Allocator>::newName(const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &pName)
{
    if (!pName.empty())
    {
        size_t sz = pName.size() + 1;
        const char *cstr = pName.c_str();
        char *newName = typename allocator_type::template rebind<char>::other(allocator).allocate(sz);
        std::memcpy(newName, cstr, sz);
        return newName;
    }
    else
    {
        return 0;
    }
}

template <class _Allocator> Property<_Allocator> &Property<_Allocator>::operator=(const Property &other)
{
    setNameAndAbstractValue(other.getName(), other.getAbstractValue());
    return *this;
}

template <class _Allocator> const char *Property<_Allocator>::getName() const noexcept { return name == 0 ? "" : name; }

template <class _Allocator> template <class _Value> _Value &Property<_Allocator>::getValue()
{
    return dynamic_cast<PropertyValue<_Allocator, _Value> &>(getAbstractValue()).value;
}

template <class _Allocator> template <class _Value> const _Value &Property<_Allocator>::getValue() const
{
    return dynamic_cast<const PropertyValue<_Allocator, _Value> &>(getAbstractValue()).value;
}

template <class _Allocator>
typename Property<_Allocator>::AbstractValue &Property<_Allocator>::getAbstractValue() noexcept
{
    return value == 0 ? PropertyNullValue::getInstance() : *value;
}

template <class _Allocator>
const typename Property<_Allocator>::AbstractValue &Property<_Allocator>::getAbstractValue() const noexcept
{
    return value == 0 ? PropertyNullValue::getInstance() : *value;
}

template <class _Allocator>
template <class _NameStringType>
Property<_Allocator> &Property<_Allocator>::setName(const _NameStringType &pName)
{
    char *nname = newName(pName);
    if (name != 0)
    {
        typename allocator_type::template rebind<char>::other(allocator).deallocate(name, std::strlen(name) + 1);
    }
    name = nname;
    return *this;
}

template <class _Allocator> template <class _Value> Property<_Allocator> &Property<_Allocator>::setValue()
{
    AbstractValue *oldValue = value;
    value = new (typename _Allocator::template rebind<PropertyValue<_Allocator, _Value>>::other(allocator).allocate(1))
        PropertyValue<_Allocator, _Value>(*this);
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator>
template <class _Value, class _ValueInit>
Property<_Allocator> &Property<_Allocator>::setValue(const _ValueInit &valueInit)
{
    AbstractValue *oldValue = value;
    value = new (typename _Allocator::template rebind<PropertyValue<_Allocator, _Value>>::other(allocator).allocate(1))
        PropertyValue<_Allocator, _Value>(*this, valueInit);
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator> Property<_Allocator> &Property<_Allocator>::setAbstractValue(const AbstractValue &pValue)
{
    AbstractValue *oldValue = value;
    value = pValue.clone(*this);
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator>
template <class _Value, class _NameStringType>
Property<_Allocator> &Property<_Allocator>::setNameAndValue(const _NameStringType &pName)
{
    AbstractValue *oldValue = value;
    value = new (typename _Allocator::template rebind<PropertyValue<_Allocator, _Value>>::other(allocator).allocate(1))
        PropertyValue<_Allocator, _Value>(*this);
    try
    {
        setName(pName);
    }
    catch (std::bad_alloc &)
    {
        delete value;
        value = oldValue;
        throw;
    }
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator>
template <class _Value, class _ValueInit, class _NameStringType>
Property<_Allocator> &Property<_Allocator>::setNameAndValue(const _NameStringType &pName, const _ValueInit &pValue)
{
    AbstractValue *oldValue = value;
    value = new (typename _Allocator::template rebind<PropertyValue<_Allocator, _Value>>::other(allocator).allocate(1))
        PropertyValue<_Allocator, _Value>(*this, pValue);
    try
    {
        setName(pName);
    }
    catch (std::bad_alloc &)
    {
        delete value;
        value = oldValue;
        throw;
    }
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator>
template <class _NameStringType>
Property<_Allocator> &Property<_Allocator>::setNameAndAbstractValue(const _NameStringType &pName,
                                                                    const AbstractValue &pValue)
{
    AbstractValue *oldValue = value;
    value = pValue.clone(*this);
    try
    {
        setName(pName);
    }
    catch (std::bad_alloc &)
    {
        if (value != 0)
        {
            delete value;
        }
        value = oldValue;
        throw;
    }
    if (oldValue != 0)
    {
        delete oldValue;
    }
    return *this;
}

template <class _Allocator> const PropertyNullValue &Property<_Allocator>::nullValue() noexcept
{
    return PropertyNullValue::getInstance();
}

template <class _Allocator>
typename Property<_Allocator>::allocator_type Property<_Allocator>::get_allocator() const noexcept
{
    return allocator_type(allocator);
}

template <class _Allocator> std::ostream &Property<_Allocator>::toOStream(std::ostream &os) const
{
    os << getName();
    if (nullValue() != getAbstractValue())
    {
        getAbstractValue().toOStream(os << '=');
    }
    return os;
}

template <class _Allocator>
Property<_Allocator>::Collection::Collection(const allocator_type &pAllocator) : Property<_Allocator>(pAllocator)
{
}

template <class _Allocator>
Property<_Allocator>::Collection::Collection(const Property<_Allocator> &other) : Property<_Allocator>(other)
{
}

template <class _Allocator>
Property<_Allocator>::Collection::Collection(const Collection &other)
    : MultiForwardChainLink<Collection>(), Property<_Allocator>(other.get_allocator())
{
    operator=(other);
}

template <class _Allocator> Property<_Allocator>::Collection::~Collection() noexcept { clear(); }

template <class _Allocator>
typename Property<_Allocator>::Collection &Property<_Allocator>::Collection::
operator=(const Property<_Allocator> &other)
{
    this->Property<_Allocator>::operator=(other);
    return *this;
}

template <class _Allocator>
typename Property<_Allocator>::Collection &Property<_Allocator>::Collection::operator=(const Collection &other)
{
    Chain newLeaves;
    typename allocator_type::template rebind<Collection>::other allocator(get_allocator());
    try
    {
        for (const_iterator i = other.begin(), endi = other.end(); i != endi; ++i)
        {
            Collection *collectionBuffer = allocator.allocate(1);
            try
            {
                newLeaves.push_back(new (collectionBuffer) Collection(allocator));
            }
            catch (std::bad_alloc &)
            {
                allocator.deallocate(collectionBuffer, 1);
                throw;
            }
            *newLeaves.back() = *i;
        }
        this->Property<_Allocator>::operator=(other);
    }
    catch (std::bad_alloc &)
    {
        while (!newLeaves.empty())
        {
            Collection *collection = newLeaves.pop_front();
            collection->~Collection();
            allocator.deallocate(collection, 1);
        }
        throw;
    }
    newLeaves.swap(leaves);
    while (!newLeaves.empty())
    {
        Collection *collection = newLeaves.pop_front();
        collection->~Collection();
        allocator.deallocate(collection, 1);
    }
    return *this;
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::begin() noexcept
{
    return leaves.begin();
}

template <class _Allocator>
typename Property<_Allocator>::Collection::const_iterator Property<_Allocator>::Collection::begin() const noexcept
{
    return leaves.begin();
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::end() noexcept
{
    return leaves.end();
}

template <class _Allocator>
typename Property<_Allocator>::Collection::const_iterator Property<_Allocator>::Collection::end() const noexcept
{
    return leaves.end();
}

template <class _Allocator> bool Property<_Allocator>::Collection::empty() const noexcept { return leaves.empty(); }

template <class _Allocator>
template <class _NameStringType>
typename Property<_Allocator>::Collection::iterator
Property<_Allocator>::Collection::find(const _NameStringType &pName) noexcept
{
    return find(begin(), pName);
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::find(iterator from,
                                                                                           const char *pName) noexcept
{
    iterator i = from, endi = end();
    for (; i != endi && strcmp(i->getName(), pName) != 0; ++i)
    {
    }
    return i;
}

template <class _Allocator>
template <class _NameStringAllocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::find(
    iterator from, const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &pName) noexcept
{
    iterator i = from, endi = end();
    for (; i != endi && i->getName() != pName; ++i)
    {
    }
    return i;
}

template <class _Allocator>
template <class _NameStringType>
typename Property<_Allocator>::Collection::const_iterator
Property<_Allocator>::Collection::find(const _NameStringType &pName) const noexcept
{
    return find(begin(), pName);
}

template <class _Allocator>
typename Property<_Allocator>::Collection::const_iterator
Property<_Allocator>::Collection::find(const_iterator from, const char *pName) const noexcept
{
    const_iterator i = from, endi = end();
    for (; i != endi && strcmp(i->getName(), pName) != 0; ++i)
    {
    }
    return i;
}

template <class _Allocator>
template <class _NameStringAllocator>
typename Property<_Allocator>::Collection::const_iterator Property<_Allocator>::Collection::find(
    const_iterator from, const std::basic_string<char, std::char_traits<char>, _NameStringAllocator> &pName) const
    noexcept
{
    const_iterator i = from, endi = end();
    for (; i != endi && i->getName() != pName; ++i)
    {
    }
    return i;
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::insert()
{
    return insert(end());
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::insert(iterator pos)
{
    typename allocator_type::template rebind<Collection>::other allocator(get_allocator());
    Collection *collectionBuffer = allocator.allocate(1);
    try
    {
        return leaves.insert(pos, new (collectionBuffer) Collection(allocator));
    }
    catch (std::bad_alloc &)
    {
        allocator.deallocate(collectionBuffer, 1);
        throw;
    }
}

template <class _Allocator>
template <class _NameStringType>
typename Property<_Allocator>::Collection::iterator
Property<_Allocator>::Collection::update(const _NameStringType &pName)
{
    return update(begin(), pName);
}

template <class _Allocator>
template <class _NameStringType>
typename Property<_Allocator>::Collection::iterator
Property<_Allocator>::Collection::update(iterator from, const _NameStringType &pName)
{
    iterator i = find(from, pName);
    if (i == end())
    {
        i = insert();
        try
        {
            i->setName(pName);
        }
        catch (std::bad_alloc &)
        {
            erase(i);
            throw;
        }
    }
    return i;
}

template <class _Allocator>
template <class _Value, class _NameStringType>
std::pair<_Value *, typename Property<_Allocator>::Collection::iterator>
Property<_Allocator>::Collection::updateValue(const _NameStringType &pName)
{
    return updateValue<_Value>(begin(), pName);
}

template <class _Allocator>
template <class _Value, class _NameStringType>
std::pair<_Value *, typename Property<_Allocator>::Collection::iterator>
Property<_Allocator>::Collection::updateValue(iterator from, const _NameStringType &pName)
{
    iterator i = find(from, pName);
    if (i == end())
    {
        i = insert();
        try
        {
            i->template setNameAndValue<_Value>(pName, _Value());
        }
        catch (std::bad_alloc &)
        {
            erase(i);
            throw;
        }
    }
    return std::make_pair(&i->template getValue<_Value>(), i);
}

template <class _Allocator>
typename Property<_Allocator>::Collection::iterator Property<_Allocator>::Collection::erase(iterator pos) noexcept
{
    Collection &collection = *pos;
    iterator ret = leaves.erase(pos);
    collection.~Collection();
    typename allocator_type::template rebind<Collection>::other(get_allocator()).deallocate(&collection, 1);
    return ret;
}

template <class _Allocator> void Property<_Allocator>::Collection::clear() noexcept
{
    typename allocator_type::template rebind<Collection>::other allocator(get_allocator());
    while (!leaves.empty())
    {
        Collection *collection = leaves.pop_front();
        collection->~Collection();
        allocator.deallocate(collection, 1);
    }
}

template <class _Allocator>
std::ostream &Property<_Allocator>::Collection::toOStream(std::ostream &os, bool first) const
{
    bool headerEmpty = *getName() == '\0' && nullValue() == getAbstractValue();
    if (first && (!headerEmpty || leaves.empty()))
    {
        os << '{';
    }
    this->Property<_Allocator>::toOStream(os);
    if (!leaves.empty())
    {
        if (!headerEmpty)
        {
            os << ':';
        }
        os << '{';
        for (const_iterator i = begin(), endi = end(); i != endi;)
        {
            i->toOStream(os, false);
            if (++i != endi)
            {
                os << ',';
            }
        }
        os << '}';
    }
    if (first && (!headerEmpty || leaves.empty()))
    {
        os << '}';
    }
    return os;
}

} // namespace