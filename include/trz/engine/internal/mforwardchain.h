/**
 * @file mforwardchain.h
 * @brief Multi forward linked list
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <cassert>
#include <cstddef>

#include "trz/engine/platform.h"

namespace tredzone
{

template <class _Item, unsigned _CHAIN_COUNT = 1u> class MultiForwardChainLink;
template <class _Item, unsigned _CHAIN_COUNT = 1u> struct MultiForwardChainItemAccessor;
template <class _Item, unsigned _CHAIN_COUNT = 1u, unsigned _CHAIN_ID = 0u> class MultiForwardChainLinkAccessor;
template <class _Item, unsigned _CHAIN_COUNT = 1u, unsigned _CHAIN_ID = 0u,
          class _ItemAccessor = MultiForwardChainItemAccessor<_Item, _CHAIN_COUNT>>
class MultiForwardChain;

template <class _Item, unsigned _CHAIN_COUNT> struct MultiForwardChainItemAccessor
{
    inline static _Item *getItem(MultiForwardChainLink<_Item, _CHAIN_COUNT> *link) noexcept
    {
        return static_cast<_Item *>(link);
    }
    inline static const _Item *getItem(const MultiForwardChainLink<_Item, _CHAIN_COUNT> *link) noexcept
    {
        return static_cast<const _Item *>(link);
    }
    inline static MultiForwardChainLink<_Item, _CHAIN_COUNT> *getLink(_Item *item) noexcept
    {
        return static_cast<MultiForwardChainLink<_Item, _CHAIN_COUNT> *>(item);
    }
    inline static const MultiForwardChainLink<_Item, _CHAIN_COUNT> *getLink(const _Item *item) noexcept
    {
        return static_cast<const MultiForwardChainLink<_Item, _CHAIN_COUNT> *>(item);
    }
};

template <class _Item> class MultiForwardChainLink<_Item, 1u>
{
  public:
    template <unsigned _CHAIN_ID = 0u, class _ItemAccessor = MultiForwardChainItemAccessor<_Item, 1u>>
    struct ForwardChain : public MultiForwardChain<_Item, 1u, _CHAIN_ID, _ItemAccessor>
    {
    };

#ifndef NDEBUG
    MultiForwardChainLink() : next(this) {}
    MultiForwardChainLink(const MultiForwardChainLink &) : next(this) {}
#endif
    inline MultiForwardChainLink &operator=(const MultiForwardChainLink &) noexcept { return *this; }

  private:
    template <class, unsigned, unsigned> friend class MultiForwardChainLinkAccessor;
    MultiForwardChainLink *next;
};

template <class _Item> class MultiForwardChainLinkAccessor<_Item, 1u, 0u>
{
    template <class, unsigned, unsigned, class> friend class MultiForwardChain;
    inline static MultiForwardChainLink<_Item, 1u> *getNext(MultiForwardChainLink<_Item, 1u> *item)
    {
        return item->next;
    }
    inline static void setNext(MultiForwardChainLink<_Item, 1u> *item, MultiForwardChainLink<_Item, 1u> *const next)
    {
        item->next = next;
    }
};

template <class _Item> class MultiForwardChainLink<_Item, 2u>
{
  public:
    template <unsigned _CHAIN_ID, class _ItemAccessor = MultiForwardChainItemAccessor<_Item, 2u>>
    struct ForwardChain : public MultiForwardChain<_Item, 2u, _CHAIN_ID, _ItemAccessor>
    {
    };

#ifndef NDEBUG
    MultiForwardChainLink() : next0(this), next1(this) {}
    MultiForwardChainLink(const MultiForwardChainLink &) : next0(this), next1(this) {}
#endif

  private:
    template <class, unsigned, unsigned> friend class MultiForwardChainLinkAccessor;
    MultiForwardChainLink *next0;
    MultiForwardChainLink *next1;
};

template <class _Item> class MultiForwardChainLinkAccessor<_Item, 2u, 0u>
{
    template <class, unsigned, unsigned, class> friend class MultiForwardChain;
    inline static MultiForwardChainLink<_Item, 2u> *getNext(MultiForwardChainLink<_Item, 2u> *item)
    {
        return item->next0;
    }
    inline static void setNext(MultiForwardChainLink<_Item, 2u> *item, MultiForwardChainLink<_Item, 2u> *const next)
    {
        item->next0 = next;
    }
};

template <class _Item> class MultiForwardChainLinkAccessor<_Item, 2u, 1u>
{
    template <class, unsigned, unsigned, class> friend class MultiForwardChain;
    inline static MultiForwardChainLink<_Item, 2u> *getNext(MultiForwardChainLink<_Item, 2u> *item)
    {
        return item->next1;
    }
    inline static void setNext(MultiForwardChainLink<_Item, 2u> *item, MultiForwardChainLink<_Item, 2u> *const next)
    {
        item->next1 = next;
    }
};

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor> class MultiForwardChain
{
  public:
    typedef _Item value_type;
    typedef _Item &reference;
    typedef const _Item &const_reference;
    typedef _Item *pointer;
    typedef const _Item *const_pointer;
    template <class _Value> class iterator_base;
    typedef iterator_base<_Item> iterator;             //!< forward, dereferences to reference
    typedef iterator_base<const _Item> const_iterator; //!< forward, dereferences to const_reference

    inline MultiForwardChain() noexcept;
    inline iterator insert(const iterator &, pointer) noexcept;
    inline iterator erase(const iterator &) noexcept;
    inline void push_front(pointer) noexcept;
    inline void push_front(MultiForwardChain &) noexcept;
    inline void push_back(pointer) noexcept;
    inline void push_back(MultiForwardChain &) noexcept;
    inline pointer pop_front() noexcept;
    inline void swap(MultiForwardChain &) noexcept;
    inline bool empty() const noexcept;
    inline iterator begin() noexcept;
    inline const_iterator begin() const noexcept;
    inline iterator end() noexcept;
    inline const_iterator end() const noexcept;
    inline pointer front() noexcept;
    inline const_pointer front() const noexcept;
    inline pointer back() noexcept;
    inline const_pointer back() const noexcept;

  private:
    typedef MultiForwardChainLinkAccessor<_Item, _CHAIN_COUNT, _CHAIN_ID> LinkAccessor;
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *head;
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *tail;

    MultiForwardChain(const MultiForwardChain &);            // illegal (can not copy, only move)
    MultiForwardChain &operator=(const MultiForwardChain &); // illegal (can not copy, only move)
};

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
template <class _Value>
class MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator_base
{
  public:
    typedef std::forward_iterator_tag iterator_category;
    typedef _Value value_type;
    typedef ptrdiff_t difference_type;
    typedef ptrdiff_t distance_type;
    typedef value_type *pointer;
    typedef value_type &reference;

    inline iterator_base() noexcept : previous(0), current(0) {}
    inline iterator_base(const iterator_base &other) noexcept : previous(other.previous), current(other.current) {}
    inline iterator_base &operator=(const iterator_base &other) noexcept
    {
        previous = other.previous;
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
        previous = current;
        current = LinkAccessor::getNext(current);
        return *this;
    }
    inline iterator_base operator++(int)noexcept
    {
        iterator_base ret = *this;
        operator++();
        return ret;
    }

  private:
    friend class MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>;
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *previous;
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *current;
    inline iterator_base(MultiForwardChainLink<_Item, _CHAIN_COUNT> *pcurrent,
                         MultiForwardChainLink<_Item, _CHAIN_COUNT> *pprevious) noexcept : previous(pprevious),
                                                                                           current(pcurrent)
    {
    }
};

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::MultiForwardChain() noexcept : head(0), tail(0)
{
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::insert(const iterator &i, pointer item) noexcept
{
    /*register*/ MultiForwardChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(item != 0);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
    if (i.previous == 0)
    {
        head = itemLink;
    }
    else
    {
        LinkAccessor::setNext(i.previous, itemLink);
    }
    if (i.current == 0)
    {
        tail = itemLink;
    }
    LinkAccessor::setNext(itemLink, i.current);
    return iterator(itemLink, i.previous);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::erase(const iterator &i) noexcept
{
    assert(!empty());
    assert(i.current != 0);
    assert(LinkAccessor::getNext(i.current) != i.current);
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *next = LinkAccessor::getNext(i.current);
    if (i.previous == 0)
    {
        head = next;
    }
    else
    {
        LinkAccessor::setNext(i.previous, next);
    }
    if (i.current == tail)
    {
        tail = i.previous;
    }
#ifndef NDEBUG
    LinkAccessor::setNext(i.current, i.current);
#endif
    return iterator(next, i.previous);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_front(pointer item) noexcept
{
    /*register*/ MultiForwardChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
#ifndef NDEBUG
    LinkAccessor::setNext(itemLink, itemLink + 1);
#endif
    if (head == 0)
    {
        head = tail = itemLink;
    }
    else
    {
        LinkAccessor::setNext(itemLink, head);
        head = itemLink;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_front(MultiForwardChain &other) noexcept
{
    if (empty() || other.empty())
    {
        if (!other.empty())
        {
            swap(other);
        }
    }
    else
    {
        LinkAccessor::setNext(other.tail, head);
        head = other.head;
        other.head = 0;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_back(pointer item) noexcept
{
    /*register*/ MultiForwardChainLink<_Item, _CHAIN_COUNT> *itemLink = _ItemAccessor::getLink(item);
    assert(LinkAccessor::getNext(itemLink) == itemLink);
#ifndef NDEBUG
    LinkAccessor::setNext(itemLink, itemLink + 1);
#endif
    if (head == 0)
    {
        head = tail = itemLink;
    }
    else
    {
        LinkAccessor::setNext(tail, itemLink);
        tail = itemLink;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::push_back(MultiForwardChain &other) noexcept
{
    if (empty() || other.empty())
    {
        if (!other.empty())
        {
            swap(other);
        }
    }
    else
    {
        LinkAccessor::setNext(tail, other.head);
        tail = other.tail;
        other.head = 0;
    }
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pop_front() noexcept
{
    assert(!empty());
    MultiForwardChainLink<_Item, _CHAIN_COUNT> *ret = head;
    if (head == tail)
    {
        head = tail = 0;
    }
    else
    {
        head = LinkAccessor::getNext(head);
    }
#ifndef NDEBUG
    LinkAccessor::setNext(ret, ret);
#endif
    return _ItemAccessor::getItem(ret);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
void MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::swap(MultiForwardChain &other) noexcept
{
    std::swap(head, other.head);
    std::swap(tail, other.tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
bool MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::empty() const noexcept
{
    return head == 0;
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::begin() noexcept
{
    assert((head == 0 && tail == 0) || (head != 0 && tail != 0));
    if (tail != 0)
    {
        LinkAccessor::setNext(tail, 0);
    }
    return iterator(head, 0);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::begin() const noexcept
{
    assert((head == 0 && tail == 0) || (head != 0 && tail != 0));
    if (tail != 0)
    {
        LinkAccessor::setNext(tail, 0);
    }
    return const_iterator(head, 0);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::end() noexcept
{
    assert((head == 0 && tail == 0) || (head != 0 && tail != 0));
    return iterator(0, tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_iterator
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::end() const noexcept
{
    assert((head == 0 && tail == 0) || (head != 0 && tail != 0));
    return const_iterator(0, tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::front() noexcept
{
    return _ItemAccessor::getItem(head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_pointer
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::front() const noexcept
{
    return _ItemAccessor::getItem(head);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::pointer
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::back() noexcept
{
    return _ItemAccessor::getItem(tail);
}

template <class _Item, unsigned _CHAIN_COUNT, unsigned _CHAIN_ID, class _ItemAccessor>
typename MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::const_pointer
MultiForwardChain<_Item, _CHAIN_COUNT, _CHAIN_ID, _ItemAccessor>::back() const noexcept
{
    return _ItemAccessor::getItem(tail);
}

} // namespace