/**
 * @file mdoublechain.h
 * @brief Multi double linked list
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cassert>
#include <cstddef>

#include "trz/engine/platform.h"

namespace tredzone
{

template <class _Item, unsigned _CHAIN_COUNT = 1u> class MultiDoubleChainLink;
template <class _Item, unsigned _CHAIN_COUNT = 1u> struct MultiDoubleChainItemAccessor;
template <class _Item, unsigned _CHAIN_COUNT = 1u, unsigned _CHAIN_ID = 0u> class MultiDoubleChainLinkAccessor;
template <class _Item, unsigned _CHAIN_COUNT = 1u, unsigned _CHAIN_ID = 0u,
          class _ItemAccessor = MultiDoubleChainItemAccessor<_Item, _CHAIN_COUNT>>
class MultiDoubleChain;

//---- Multi-Double-Chain ITEM ACCESSOR ----------------------------------------

template <class _Item, unsigned _CHAIN_COUNT>
struct MultiDoubleChainItemAccessor
{
    inline static _Item *getItem(MultiDoubleChainLink<_Item, _CHAIN_COUNT> *link) noexcept
    {
        return static_cast<_Item *>(link);
    }
    inline static const _Item *getItem(const MultiDoubleChainLink<_Item, _CHAIN_COUNT> *link) noexcept
    {
        return static_cast<const _Item *>(link);
    }
    inline static MultiDoubleChainLink<_Item, _CHAIN_COUNT> *getLink(_Item *item) noexcept
    {
        return static_cast<MultiDoubleChainLink<_Item, _CHAIN_COUNT> *>(item);
    }
    inline static const MultiDoubleChainLink<_Item, _CHAIN_COUNT> *getLink(const _Item *item) noexcept
    {
        return static_cast<const MultiDoubleChainLink<_Item, _CHAIN_COUNT> *>(item);
    }
};

//---- Multi-Double-Chain LINK -------------------------------------------------

template<class _Item>
class MultiDoubleChainLink<_Item, 1u>
{
  public:
    template <unsigned _CHAIN_ID = 0u, class _ItemAccessor = MultiDoubleChainItemAccessor<_Item, 1u>>
    struct DoubleChain : public MultiDoubleChain<_Item, 1u, _CHAIN_ID, _ItemAccessor>
    {
    };

#ifndef NDEBUG
    // infinite link?
    MultiDoubleChainLink() : previous(this), next(this) {}
    MultiDoubleChainLink(const MultiDoubleChainLink &) : previous(this), next(this) {}
#endif
    inline MultiDoubleChainLink &operator=(const MultiDoubleChainLink &) noexcept { return *this; }

  private:
    template <class, unsigned, unsigned> friend class MultiDoubleChainLinkAccessor;
    MultiDoubleChainLink *previous;
    MultiDoubleChainLink *next;
};

//---- Multi-Double-Chain LINK ACCESSOR ----------------------------------------

template<class _Item>
class MultiDoubleChainLinkAccessor<_Item, 1u, 0u>
{
    template <class, unsigned, unsigned, class> friend class MultiDoubleChain;
    inline static MultiDoubleChainLink<_Item, 1u> *getPrevious(MultiDoubleChainLink<_Item, 1u> *item)
    {
        return item->previous;
    }
    inline static MultiDoubleChainLink<_Item, 1u> *getNext(MultiDoubleChainLink<_Item, 1u> *item) { return item->next; }
    inline static void setPrevious(MultiDoubleChainLink<_Item, 1u> *item,
                                   MultiDoubleChainLink<_Item, 1u> *const previous)
    {
        item->previous = previous;
    }
    inline static void setNext(MultiDoubleChainLink<_Item, 1u> *item, MultiDoubleChainLink<_Item, 1u> *const next)
    {
        item->next = next;
    }
};

//---- Multi-Double-Chain LINK -------------------------------------------------

template<class _Item>
class MultiDoubleChainLink<_Item, 2u>
{
  public:
    template <unsigned _CHAIN_ID, class _ItemAccessor = MultiDoubleChainItemAccessor<_Item, 2u>>
    struct DoubleChain : public MultiDoubleChain<_Item, 2u, _CHAIN_ID, _ItemAccessor>
    {
    };

#ifndef NDEBUG
    MultiDoubleChainLink() : previous0(this), next0(this), previous1(this), next1(this) {}
    MultiDoubleChainLink(const MultiDoubleChainLink &) : previous0(this), next0(this), previous1(this), next1(this) {}
#endif
    inline MultiDoubleChainLink &operator=(const MultiDoubleChainLink &) noexcept { return *this; }

  private:
    template <class, unsigned, unsigned> friend class MultiDoubleChainLinkAccessor;
    MultiDoubleChainLink *previous0;
    MultiDoubleChainLink *next0;
    MultiDoubleChainLink *previous1;
    MultiDoubleChainLink *next1;
};

//---- Multi-Double-Chain LINK ACCESSOR (2/0 specialization) -------------------

template<class _Item>
class MultiDoubleChainLinkAccessor<_Item, 2u, 0u>
{
    template <class, unsigned, unsigned, class> friend class MultiDoubleChain;
    inline static MultiDoubleChainLink<_Item, 2u> *getPrevious(MultiDoubleChainLink<_Item, 2u> *item)
    {
        return item->previous0;
    }
    inline static MultiDoubleChainLink<_Item, 2u> *getNext(MultiDoubleChainLink<_Item, 2u> *item)
    {
        return item->next0;
    }
    inline static void setPrevious(MultiDoubleChainLink<_Item, 2u> *item,
                                   MultiDoubleChainLink<_Item, 2u> *const previous)
    {
        item->previous0 = previous;
    }
    inline static void setNext(MultiDoubleChainLink<_Item, 2u> *item, MultiDoubleChainLink<_Item, 2u> *const next)
    {
        item->next0 = next;
    }
};

//---- Multi-Double-Chain LINK ACCESSOR (2/1 specialization) -------------------

template<class _Item>
class MultiDoubleChainLinkAccessor<_Item, 2u, 1u>
{
    template <class, unsigned, unsigned, class> friend class MultiDoubleChain;
    inline static MultiDoubleChainLink<_Item, 2u> *getPrevious(MultiDoubleChainLink<_Item, 2u> *item)
    {
        return item->previous1;
    }
    inline static MultiDoubleChainLink<_Item, 2u> *getNext(MultiDoubleChainLink<_Item, 2u> *item)
    {
        return item->next1;
    }
    inline static void setPrevious(MultiDoubleChainLink<_Item, 2u> *item,
                                   MultiDoubleChainLink<_Item, 2u> *const previous)
    {
        item->previous1 = previous;
    }
    inline static void setNext(MultiDoubleChainLink<_Item, 2u> *item, MultiDoubleChainLink<_Item, 2u> *const next)
    {
        item->next1 = next;
    }
};

//---- Multi-Double-Chain ------------------------------------------------------

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
class MultiDoubleChain
{
  public:
    typedef _Item value_type;
    typedef _Item &reference;
    typedef const _Item &const_reference;
    typedef _Item *pointer;
    typedef const _Item *const_pointer;
    template <class _Chain, class _Value> class iterator_base;
    typedef iterator_base<MultiDoubleChain, _Item> iterator; //!< bidirectional, dereferences to reference
    typedef iterator_base<const MultiDoubleChain, const _Item>
        const_iterator; //!< bidirectional, dereferences to const_reference
    typedef std::reverse_iterator<iterator> reverse_iterator;
    typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

    inline MultiDoubleChain() noexcept;
    inline iterator insert(const iterator &, pointer) noexcept;
    inline void remove(pointer) noexcept;
    inline iterator erase(const iterator &) noexcept;
    inline void push_front(pointer) noexcept;
    inline void push_front(MultiDoubleChain &) noexcept;
    inline void push_back(pointer) noexcept;
    inline void push_back(MultiDoubleChain &) noexcept;
    inline pointer pop_front() noexcept;
    inline pointer pop_back() noexcept;
    inline void swap(MultiDoubleChain &) noexcept;
    inline bool empty() const noexcept;
    inline iterator begin() noexcept;
    inline const_iterator begin() const noexcept;
    inline iterator end() noexcept;
    inline const_iterator end() const noexcept;
    inline reverse_iterator rbegin() noexcept;
    inline const_reverse_iterator rbegin() const noexcept;
    inline reverse_iterator rend() noexcept;
    inline const_reverse_iterator rend() const noexcept;
    inline pointer front() noexcept;
    inline const_pointer front() const noexcept;
    inline pointer back() noexcept;
    inline const_pointer back() const noexcept;

  private:
    typedef MultiDoubleChainLinkAccessor<_Item, _CHAIN_COUNT, _CHAIN_ID> LinkAccessor;
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *head;
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *tail;

    MultiDoubleChain(const MultiDoubleChain &) = delete;            // illegal (can not copy, only move)
    MultiDoubleChain &operator=(const MultiDoubleChain &) = delete; // illegal (can not copy, only move)
};

template<class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
template<class _Chain, class _Value>
class MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator_base
{
  public:
    typedef std::bidirectional_iterator_tag iterator_category;
    typedef _Value value_type;
    typedef ptrdiff_t difference_type;
    typedef ptrdiff_t distance_type;
    typedef value_type *pointer;
    typedef value_type &reference;

    inline iterator_base() noexcept : chain(0), current(0) {}
    inline iterator_base(const iterator_base &other) noexcept : chain(other.chain), current(other.current) {}
    inline iterator_base &operator=(const iterator_base &other) noexcept
    {
        chain = other.chain;
        current = other.current;
        return *this;
    }
    inline bool operator==(const iterator_base &other) const noexcept { return current == other.current; }
    inline bool operator!=(const iterator_base &other) const noexcept { return current != other.current; }
    inline reference operator*() const noexcept
    {
        assert(current != 0);
        return *_ItemAccessor::getItem(current);
    }
    inline pointer operator->() const noexcept
    {
        assert(current != 0);
        return _ItemAccessor::getItem(current);
    }
    inline iterator_base &operator++() noexcept
    {
        assert(current != 0);
        current = LinkAccessor::getNext(current);
        return *this;
    }
    inline iterator_base operator++(int)noexcept
    {
        iterator_base ret = *this;
        operator++();
        return ret;
    }
    inline iterator_base &operator--() noexcept
    {
        if (current == 0)
        {
            assert(chain != 0);
            current = chain->tail;
        }
        else
        {
            current = LinkAccessor::getPrevious(current);
        }
        return *this;
    }
    inline iterator_base operator--(int)noexcept
    {
        iterator_base ret = *this;
        operator--();
        return ret;
    }

  private:
    friend class MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>;
    _Chain *chain;
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *current;
    inline iterator_base(_Chain &pchain, MultiDoubleChainLink<_Item, _CHAIN_COUNT> *pcurrent) noexcept
        : chain(&pchain),
          current(pcurrent)
    {
    }
};

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::MultiDoubleChain() noexcept : head(0), tail(0)
{
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::insert(const iterator &i, pointer item) noexcept
{
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getPrevious(itemLink) == itemLink);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
    if (i.current == 0)
    {
        push_back(item);
    }
    else
    {
        MultiDoubleChainLink<_Item, _CHAIN_COUNT> *previousItem = LinkAccessor::getPrevious(i.current);
        if (previousItem == 0)
        {
            head = itemLink;
        }
        else
        {
            LinkAccessor::setNext(previousItem, itemLink);
        }
        LinkAccessor::setPrevious(i.current, itemLink);
        LinkAccessor::setPrevious(itemLink, previousItem);
        LinkAccessor::setNext(itemLink, i.current);
    }
    return iterator(*this, itemLink);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::remove(pointer item) noexcept
{
    /*register*/ MultiDoubleChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getPrevious(itemLink) != itemLink);
    assert(LinkAccessor::getNext(itemLink) != itemLink);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *previousItem = LinkAccessor::getPrevious(itemLink);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *nextItem = LinkAccessor::getNext(itemLink);
    if (previousItem == 0)
    {
        assert(itemLink == head);
        head = nextItem;
    }
    else
    {
        LinkAccessor::setNext(previousItem, nextItem);
    }
    if (nextItem == 0)
    {
        assert(itemLink == tail);
        tail = previousItem;
    }
    else
    {
        LinkAccessor::setPrevious(nextItem, previousItem);
    }
#ifndef NDEBUG
    LinkAccessor::setPrevious(itemLink, itemLink);
    LinkAccessor::setNext(itemLink, itemLink);
#endif
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::erase(const iterator &i) noexcept
{
    assert(i.current != 0);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *previousItem = LinkAccessor::getPrevious(i.current);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *nextItem = LinkAccessor::getNext(i.current);
    if (previousItem == 0)
    {
        assert(i.current == head);
        head = nextItem;
    }
    else
    {
        LinkAccessor::setNext(previousItem, nextItem);
    }
    if (nextItem == 0)
    {
        assert(i.current == tail);
        tail = previousItem;
    }
    else
    {
        LinkAccessor::setPrevious(nextItem, previousItem);
    }
#ifndef NDEBUG
    LinkAccessor::setPrevious(i.current, i.current);
    LinkAccessor::setNext(i.current, i.current);
#endif
    return iterator(*this, nextItem);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_front(pointer item) noexcept
{
    /*register*/ MultiDoubleChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getPrevious(itemLink) == itemLink);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
    if (head == 0)
    {
        assert(tail == 0);
        head = tail = itemLink;
        LinkAccessor::setPrevious(itemLink, 0);
        LinkAccessor::setNext(itemLink, 0);
    }
    else
    {
        LinkAccessor::setPrevious(head, itemLink);
        LinkAccessor::setPrevious(itemLink, 0);
        LinkAccessor::setNext(itemLink, head);
        head = itemLink;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_front(MultiDoubleChain &other) noexcept
{
    if (head != 0 && other.tail != 0)
    {
        assert(other.tail != 0);
        LinkAccessor::setPrevious(head, other.tail);
        LinkAccessor::setNext(other.tail, head);
        head = other.head;
        other.head = other.tail = 0;
    }
    else if (head == 0)
    {
        swap(other);
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_back(pointer item) noexcept
{
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getPrevious(itemLink) == itemLink);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
    if (head == 0)
    {
        assert(tail == 0);
        head = tail = itemLink;
        LinkAccessor::setPrevious(itemLink, 0);
        LinkAccessor::setNext(itemLink, 0);
    }
    else
    {
        LinkAccessor::setNext(tail, itemLink);
        LinkAccessor::setPrevious(itemLink, tail);
        LinkAccessor::setNext(itemLink, 0);
        tail = itemLink;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_back(MultiDoubleChain &other) noexcept
{
    if (tail != 0 && other.head != 0)
    {
        assert(other.head != 0);
        LinkAccessor::setPrevious(other.head, tail);
        LinkAccessor::setNext(tail, other.head);
        tail = other.tail;
        other.head = other.tail = 0;
    }
    else if (tail == 0)
    {
        swap(other);
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pop_front() noexcept
{
    assert(head != 0);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *ret = head;
    head = LinkAccessor::getNext(head);
    if (head == 0)
    {
        tail = 0;
    }
    else
    {
        LinkAccessor::setPrevious(head, 0);
    }
#ifndef NDEBUG
    LinkAccessor::setPrevious(ret, ret);
    LinkAccessor::setNext(ret, ret);
#endif
    return _ItemAccessor::getItem(ret);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pop_back() noexcept
{
    assert(tail != 0);
    MultiDoubleChainLink<_Item, _CHAIN_COUNT> *ret = tail;
    tail = LinkAccessor::getPrevious(tail);
    if (tail == 0)
    {
        head = 0;
    }
    else
    {
        LinkAccessor::setNext(tail, 0);
    }
#ifndef NDEBUG
    LinkAccessor::setPrevious(ret, ret);
    LinkAccessor::setNext(ret, ret);
#endif
    return _ItemAccessor::getItem(ret);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::swap(MultiDoubleChain &other) noexcept
{
    std::swap(head, other.head);
    std::swap(tail, other.tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
bool MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::empty() const noexcept
{
    return head == 0;
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::begin() noexcept
{
    return iterator(*this, head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::begin() const noexcept
{
    return const_iterator(*this, head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::end() noexcept
{
    return iterator(*this, 0);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::end() const noexcept
{
    return const_iterator(*this, 0);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::reverse_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::rbegin() noexcept
{
    return reverse_iterator(end());
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_reverse_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::rbegin() const noexcept
{
    return const_reverse_iterator(end());
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::reverse_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::rend() noexcept
{
    return reverse_iterator(begin());
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_reverse_iterator
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::rend() const noexcept
{
    return const_reverse_iterator(begin());
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::front() noexcept
{
    assert(head != 0);
    return _ItemAccessor::getItem(head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::front() const noexcept
{
    assert(head != 0);
    return _ItemAccessor::getItem(head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::back() noexcept
{
    assert(tail != 0);
    return _ItemAccessor::getItem(tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_pointer
MultiDoubleChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::back() const noexcept
{
    assert(tail != 0);
    return _ItemAccessor::getItem(tail);
}

} // namespace