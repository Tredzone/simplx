/**
 * @file testmforwardchain.cpp
 * @brief test multi forward chain
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <list>
#include <stdint.h>

#include "gtest/gtest.h"

#include "trz/engine/internal/mforwardchain.h"

template <unsigned _CHAIN_COUNT> class TestChainWithOneLink;

template <unsigned _CHAIN_COUNT>
struct TestItemWithOneLink : private tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT>
{
    template <unsigned> friend class TestChainWithOneLink;

    const uintptr_t v;

    TestItemWithOneLink() : v((uintptr_t) this) {}
    TestItemWithOneLink(const TestItemWithOneLink &)
        : tredzone::MultiForwardChainLink<TestItemWithOneLink, _CHAIN_COUNT>(), v((uintptr_t) this)
    {
    }
};

template <unsigned _CHAIN_COUNT> class TestChainWithOneLink
{
  public:
    template <unsigned _CHAIN_ID = 0u>
    struct ForwardChain
        : tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>,
                                          _CHAIN_COUNT>::template ForwardChain<_CHAIN_ID, ForwardChain<_CHAIN_ID>>
    {
        inline static TestItemWithOneLink<_CHAIN_COUNT> *getItem(
            typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT> *link) noexcept
        {
            return static_cast<TestItemWithOneLink<_CHAIN_COUNT> *>(link);
        }
        inline static const TestItemWithOneLink<_CHAIN_COUNT> *
        getItem(const typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT>
                    *link) noexcept
        {
            return static_cast<const TestItemWithOneLink<_CHAIN_COUNT> *>(link);
        }
        inline static typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT> *
        getLink(TestItemWithOneLink<_CHAIN_COUNT> *item) noexcept
        {
            return static_cast<
                typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT> *>(item);
        }
        inline static const typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT> *
        getLink(const TestItemWithOneLink<_CHAIN_COUNT> *item) noexcept
        {
            return static_cast<
                const typename tredzone::MultiForwardChainLink<TestItemWithOneLink<_CHAIN_COUNT>, _CHAIN_COUNT> *>(
                item);
        }
    };

  private:
    typedef std::list<TestItemWithOneLink<_CHAIN_COUNT>> ControlList;
    ControlList controlList;
    typedef ForwardChain<> Chain;
    Chain chain;
    typedef typename ControlList::iterator iterator;

  public:
    TestChainWithOneLink() { control(); }
    void control(const typename ControlList::const_iterator &beginilist,
                 const typename Chain::const_iterator &beginichain,
                 const typename Chain::const_iterator &endichain) const
    {
        ASSERT_EQ(controlList.empty(), chain.empty());
        if (!controlList.empty())
        {
            ASSERT_EQ(&controlList.front(), chain.front());
            ASSERT_EQ(&controlList.back(), chain.back());
        }
        if (controlList.size() > 1)
        {
            typename Chain::const_iterator i = chain.begin();
            ASSERT_EQ(&*(i++), &*controlList.begin());
            ASSERT_EQ(&*i, &*(++(controlList.begin())));
            i = chain.begin();
            ASSERT_EQ(&*(++i), &*(++(controlList.begin())));
        }
        typename ControlList::const_iterator ilist = beginilist, endilist = controlList.end();
        typename Chain::const_iterator ichain = beginichain;
        for (; ilist != endilist && ichain != endichain; ++ilist, ++ichain)
        {
            ASSERT_EQ(&*ichain, &*ilist);
            ASSERT_EQ(ichain->v, ilist->v);
            if (ilist == controlList.begin())
            {
                ASSERT_EQ(ichain, chain.begin());
            }
            else
            {
                ASSERT_NE(ichain, chain.begin());
            }
        }
        ASSERT_EQ(ilist, endilist);
        ASSERT_EQ(ichain, endichain);
    }
    void control(const typename ControlList::iterator &beginilist, const typename Chain::iterator &beginichain,
                 const typename Chain::iterator &endichain)
    {
        ASSERT_EQ(controlList.empty(), chain.empty());
        if (!controlList.empty())
        {
            ASSERT_EQ(&controlList.front(), chain.front());
            ASSERT_EQ(&controlList.back(), chain.back());
        }
        if (controlList.size() > 1)
        {
            typename Chain::iterator i = chain.begin();
            ASSERT_EQ(&*(i++), &*controlList.begin());
            ASSERT_EQ(&*i, &*(++(controlList.begin())));
            i = chain.begin();
            ASSERT_EQ(&*(++i), &*(++(controlList.begin())));
        }
        typename ControlList::iterator ilist = beginilist, endilist = controlList.end();
        typename Chain::iterator ichain = beginichain;
        for (; ilist != endilist && ichain != endichain; ++ilist, ++ichain)
        {
            ASSERT_EQ(&*ichain, &*ilist);
            ASSERT_EQ(ichain->v, ilist->v);
            if (ilist == controlList.begin())
            {
                ASSERT_EQ(ichain, chain.begin());
            }
            else
            {
                ASSERT_NE(ichain, chain.begin());
            }
        }
        ASSERT_EQ(ilist, endilist);
        ASSERT_EQ(ichain, endichain);
    }
    void control()
    {
        control(controlList.begin(), chain.begin(), chain.end());
        const_control();
    }
    void const_control()
    {
        static_cast<const TestChainWithOneLink *>(this)->control(static_cast<const ControlList &>(controlList).begin(),
                                                                 static_cast<const Chain &>(chain).begin(),
                                                                 static_cast<const Chain &>(chain).end());
    }
    void controlWithoutCallingChainEnd()
    {
        control(controlList.begin(), chain.begin(), typename Chain::iterator());
        static_cast<const TestChainWithOneLink *>(this)->control(static_cast<const ControlList &>(controlList).begin(),
                                                                 static_cast<const Chain &>(chain).begin(),
                                                                 typename Chain::const_iterator());
    }
    void controlWithoutCallingChainEnd(const typename ControlList::iterator &beginilist,
                                       const typename Chain::iterator &beginichain)
    {
        control(beginilist, beginichain, typename Chain::iterator());
    }
    void push_back()
    {
        controlList.push_back(TestItemWithOneLink<_CHAIN_COUNT>());
        chain.push_back(&controlList.back());
        control();
    }
    void push_back(TestChainWithOneLink &other)
    {
        Chain temp;
        for (typename ControlList::iterator i = other.controlList.begin(), endi = other.controlList.end(); i != endi;
             ++i)
        {
            controlList.push_back(*i);
            temp.push_back(&controlList.back());
        }
        chain.push_back(temp);
        control();
        ASSERT_TRUE(temp.empty());
    }
    void push_front(TestChainWithOneLink &other)
    {
        Chain temp;
        for (typename ControlList::reverse_iterator i = other.controlList.rbegin(), endi = other.controlList.rend();
             i != endi; ++i)
        {
            controlList.push_front(*i);
            temp.push_front(&controlList.front());
        }
        chain.push_front(temp);
        control();
        ASSERT_TRUE(temp.empty());
    }
    void push_back_no_control()
    {
        controlList.push_front(TestItemWithOneLink<_CHAIN_COUNT>());
        chain.push_front(&controlList.front());
    }
    void push_front()
    {
        controlList.push_front(TestItemWithOneLink<_CHAIN_COUNT>());
        chain.push_front(&controlList.front());
        control();
        ASSERT_EQ(chain.pop_front(), &controlList.front());
        chain.push_front(&controlList.front());
        control();
    }
    void push_front_no_control()
    {
        controlList.push_front(TestItemWithOneLink<_CHAIN_COUNT>());
        chain.push_front(&controlList.front());
    }
    void pop_front()
    {
        ASSERT_EQ(controlList.empty(), chain.empty());
        ASSERT_TRUE(!controlList.empty());
        ASSERT_EQ(chain.pop_front(), &controlList.front());
        controlList.pop_front();
        control();
    }
    void insertFirst()
    {
        typename ControlList::iterator ilist =
            controlList.insert(controlList.begin(), TestItemWithOneLink<_CHAIN_COUNT>());
        typename Chain::iterator ichain = chain.insert(chain.begin(), &controlList.front());
        controlWithoutCallingChainEnd(ilist, ichain);
        controlWithoutCallingChainEnd();
    }
    void insertLast()
    {
        typename ControlList::iterator ilist =
            controlList.insert(controlList.end(), TestItemWithOneLink<_CHAIN_COUNT>());
        typename Chain::iterator ichain = chain.insert(chain.end(), &controlList.back());
        controlWithoutCallingChainEnd(ilist, ichain);
        controlWithoutCallingChainEnd();
    }
    void insertSecond()
    {
        ASSERT_TRUE(controlList.size() > 0);
        typename ControlList::iterator ilist =
            controlList.insert(++controlList.begin(), TestItemWithOneLink<_CHAIN_COUNT>());
        typename Chain::iterator ichain = chain.insert(++chain.begin(), &*(++controlList.begin()));
        controlWithoutCallingChainEnd(ilist, ichain);
        controlWithoutCallingChainEnd();
        ASSERT_TRUE(ilist != controlList.end());
        ASSERT_EQ(&*ilist, &*ichain);
    }
    void eraseFirst()
    {
        ASSERT_EQ(controlList.empty(), chain.empty());
        ASSERT_TRUE(!chain.empty());
        typename Chain::iterator ichain = chain.erase(chain.begin());
        typename ControlList::iterator ilist = controlList.erase(controlList.begin());
        controlWithoutCallingChainEnd(ilist, ichain);
        controlWithoutCallingChainEnd();
    }
    void eraseSecond()
    {
        ASSERT_TRUE(controlList.size() > 1);
        ASSERT_EQ(controlList.empty(), chain.empty());
        typename Chain::iterator ichain = chain.erase(++chain.begin());
        typename ControlList::iterator ilist = controlList.erase(++controlList.begin());
        controlWithoutCallingChainEnd(ilist, ichain);
        controlWithoutCallingChainEnd();
    }
    void swap(TestChainWithOneLink &other)
    {
        controlList.swap(other.controlList);
        chain.swap(other.chain);
        other.control();
        control();
    }
    iterator begin() { return controlList.begin(); }
    iterator end() { return controlList.begin(); }
};

template <unsigned _CHAIN_COUNT> void testPushAndPop()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_back();
        test.push_front();
        test.pop_front();
        test.pop_front();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_front();
        test.push_back();
        test.pop_front();
        test.pop_front();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_back();
        test.pop_front();
        test.push_back();
        test.push_back();
        test.pop_front();
        test.push_back();
        test.push_back();
        test.pop_front();
        test.pop_front();
        test.pop_front();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_front();
        test.pop_front();
        test.push_front();
        test.push_front();
        test.pop_front();
        test.push_front();
        test.push_front();
        test.pop_front();
        test.pop_front();
        test.pop_front();
    }
}

void testPushAndPop()
{
    testPushAndPop<1u>();
    testPushAndPop<2u>();
}

template <unsigned _CHAIN_COUNT> void testPushChain()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test2.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test2.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test2.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test2.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }

    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test2.push_back();
        test1.push_back(test2);
        test2.push_back(test1);
        test1.push_front(test2);
        test2.push_front(test1);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.push_back();
        test1.push_back();
        test1.push_back();
        test2.push_back();
        test2.push_back();
        test1.push_front(test2);
        test2.push_front(test1);
        test1.push_back(test2);
        test2.push_back(test1);
    }
}

void testPushChain()
{
    testPushChain<1u>();
    testPushChain<2u>();
}

template <unsigned _CHAIN_COUNT> void testIterator()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_back_no_control();
        test.insertLast();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_front_no_control();
        test.insertLast();
    }
}

void testIterator()
{
    testIterator<1u>();
    testIterator<2u>();
}

template <unsigned _CHAIN_COUNT> void testInsert()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertFirst();
        test.insertFirst();
        test.insertFirst();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertLast();
        test.insertLast();
        test.insertLast();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertFirst();
        test.insertSecond();
        test.insertFirst();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertLast();
        test.insertSecond();
        test.insertLast();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertFirst();
        test.insertFirst();
        test.insertSecond();
        test.insertLast();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertLast();
        test.insertLast();
        test.insertSecond();
        test.insertFirst();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertFirst();
        test.insertLast();
        test.insertSecond();
        test.insertSecond();
        test.insertSecond();
        test.insertLast();
        test.insertSecond();
        test.insertFirst();
        test.insertSecond();
    }
}

void testInsert()
{
    testInsert<1u>();
    testInsert<2u>();
}

template <unsigned _CHAIN_COUNT> void testErase()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.push_back();
        test.eraseFirst();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertLast();
        test.eraseFirst();
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test;
        test.insertFirst();
        test.insertLast();
        test.eraseSecond();
        test.eraseFirst();
    }
}

void testErase()
{
    testErase<1u>();
    testErase<2u>();
}

template <unsigned _CHAIN_COUNT> void testSwap()
{
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test1.swap(test2);
        test1.push_back();
        test1.swap(test2);
        test1.push_front();
        test1.swap(test2);
        test1.insertFirst();
        test1.swap(test2);
        test1.insertLast();
        test1.swap(test2);
    }
    {
        TestChainWithOneLink<_CHAIN_COUNT> test1;
        TestChainWithOneLink<_CHAIN_COUNT> test2;
        test2.swap(test1);
        test1.push_back();
        test2.swap(test1);
        test1.push_front();
        test2.swap(test1);
        test1.insertFirst();
        test2.swap(test1);
        test1.insertLast();
        test2.swap(test1);
    }
}

void testSwap()
{
    testSwap<1u>();
    testSwap<2u>();
}

void testDoubleChain()
{
    TestChainWithOneLink<2u> test;
    TestChainWithOneLink<2u>::ForwardChain<1u> chain2;
    test.push_back();
    chain2.push_front(&*test.begin());
    test.control();
    ASSERT_EQ(chain2.front(), &*test.begin());
    ASSERT_EQ(chain2.back(), &*test.begin());
    ASSERT_EQ(&*chain2.begin(), &*test.begin());
    test.push_front();
    chain2.push_back(&*test.begin());
    ASSERT_EQ(chain2.front(), &*(++test.begin()));
    ASSERT_EQ(chain2.back(), &*test.begin());
    ASSERT_EQ(&*chain2.begin(), &*(++test.begin()));
    ASSERT_EQ(&*(++chain2.begin()), &*test.begin());
    ASSERT_EQ(chain2.pop_front(), &*(++test.begin()));
    test.control();
    ASSERT_EQ(chain2.front(), &*test.begin());
    ASSERT_EQ(chain2.back(), &*test.begin());
    ASSERT_EQ(&*chain2.begin(), &*test.begin());
    test.eraseSecond();
    ASSERT_EQ(chain2.pop_front(), &*test.begin());
    test.control();
    ASSERT_TRUE(chain2.empty());
}

TEST(MForwardChain, pushAndPop) { testPushAndPop(); }
TEST(MForwardChain, pushChain) { testPushChain(); }
TEST(MForwardChain, iterator) { testIterator(); }
TEST(MForwardChain, insert) { testInsert(); }
TEST(MForwardChain, erase) { testErase(); }
TEST(MForwardChain, swap) { testSwap(); }
TEST(MForwardChain, doubleChain) { testDoubleChain(); }