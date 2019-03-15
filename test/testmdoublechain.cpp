/**
 * @file testmdoublechain.cpp
 * @brief test multi double chain
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <list>

#include "gtest/gtest.h"

#include "trz/engine/internal/mdoublechain.h"

template <unsigned _CHAIN_COUNT, unsigned _CHAIN_ID> class TestSelfDoubleChainedPointers
{
  private:
    struct Value : private tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT>
    {
        struct Chain : tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT>::template DoubleChain<_CHAIN_ID, Chain>
        {
            inline static Value *getItem(tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *link) noexcept
            {
                return static_cast<Value *>(link);
            }
            inline static const Value *getItem(const tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *link) noexcept
            {
                return static_cast<const Value *>(link);
            }
            inline static tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *getLink(Value *item) noexcept
            {
                return static_cast<tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *>(item);
            }
            inline static const tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *getLink(const Value *item) noexcept
            {
                return static_cast<const tredzone::MultiDoubleChainLink<Value, _CHAIN_COUNT> *>(item);
            }
        };
        int v;
    };

    typedef typename Value::Chain Test;
    typedef std::list<Value> Control;

    Test test;
    Control control;

    template <class _TestIterator, class _ControlIterator>
    static void iteratorCheck(_TestIterator testBegin, _TestIterator testEnd, _ControlIterator controlBegin,
                              _ControlIterator controlEnd)
    {
        _TestIterator it;
        _ControlIterator ic;
        for (it = testBegin, ic = controlBegin; it != testEnd && ic != controlEnd; ++ic, ++it)
        {
            ASSERT_EQ(&*ic, &*it);
            ASSERT_EQ(&ic->v, &it->v);
            _TestIterator i = it;
            if (++i != testEnd)
            {
                ASSERT_EQ(&*(ic++), &*(it++));
                ASSERT_EQ(&(ic--)->v, &(it--)->v);
                ASSERT_EQ(&*ic, &*it);
                ASSERT_EQ(&ic->v, &it->v);
                ASSERT_EQ(&*(++ic), &*(++it));
                ASSERT_EQ(&(--ic)->v, &(--it)->v);
                ASSERT_EQ(&*ic, &*it);
                ASSERT_EQ(&ic->v, &it->v);
            }
        }
        ASSERT_EQ(ic, controlEnd);
        ASSERT_EQ(it, testEnd);
    }

    static void check(Test &test, Control &control)
    {
        const Test &const_test = test;
        const Control &const_control = control;
        iteratorCheck(test.begin(), test.end(), control.begin(), control.end());
        iteratorCheck(test.rbegin(), test.rend(), control.rbegin(), control.rend());
        iteratorCheck(const_test.begin(), const_test.end(), const_control.begin(), const_control.end());
        iteratorCheck(const_test.rbegin(), const_test.rend(), const_control.rbegin(), const_control.rend());
        ASSERT_EQ(const_test.empty(), control.empty());
        if (!test.empty())
        {
            ASSERT_EQ(test.front(), &control.front());
            ASSERT_EQ(test.back(), &control.back());
            ASSERT_EQ(const_test.front(), &const_control.front());
            ASSERT_EQ(const_test.back(), &const_control.back());
        }
    }

    TestSelfDoubleChainedPointers(int) {}

    void swap()
    {
        TestSelfDoubleChainedPointers other(0);
        test.swap(other.test);
        check(test, other.control);
        check(other.test, control);
        test.swap(other.test);
        check(test, control);
        check(other.test, other.control);

        other.control.push_back(Value());
        other.test.push_back(&other.control.back());
        check(other.test, other.control);

        test.swap(other.test);
        check(test, other.control);
        check(other.test, control);
        test.swap(other.test);
        check(test, control);
        check(other.test, other.control);
    }

    void check()
    {
        check(test, control);
        swap();
    }

  public:
    class iterator
    {
      public:
        typedef typename Control::iterator::reference reference;
        typedef typename Control::iterator::pointer pointer;

        iterator() {}

        bool operator==(const iterator &other)
        {
            EXPECT_TRUE((testIterator == other.testIterator) == (controlIterator == other.controlIterator));
            return controlIterator == other.controlIterator;
        }

        bool operator!=(const iterator &other) { return !operator==(other); }

        reference operator*() { return *controlIterator; }

        pointer operator->() { return controlIterator.operator->(); }

        iterator &operator++()
        {
            ++testIterator;
            ++controlIterator;
            return *this;
        }
        iterator operator++(int)
        {
            iterator ret = *this;
            operator++();
            return ret;
        }
        iterator &operator--()
        {
            --testIterator;
            --controlIterator;
            return *this;
        }
        iterator operator--(int)
        {
            iterator ret = *this;
            operator--();
            return ret;
        }

      private:
        template <unsigned, unsigned> friend class TestSelfDoubleChainedPointers;
        typename Test::iterator testIterator;
        typename Control::iterator controlIterator;
        iterator(const typename Test::iterator &pTestIterator, const typename Control::iterator &pControlIterator)
            : testIterator(pTestIterator), controlIterator(pControlIterator)
        {
        }
    };

    TestSelfDoubleChainedPointers() { check(); }

    void insert(iterator pos)
    {
        typename Control::iterator ic = control.insert(pos.controlIterator, Value());
        typename Test::iterator it = test.insert(pos.testIterator, &*ic);
        iteratorCheck(it, test.end(), ic, control.end());
        check();
    }

    void erase(iterator pos)
    {
        typename Test::iterator it = test.erase(pos.testIterator);
        typename Control::iterator ic = control.erase(pos.controlIterator);
        iteratorCheck(it, test.end(), ic, control.end());
        check();
    }

    void remove(iterator pos)
    {
        test.remove(&*pos.testIterator);
        control.erase(pos.controlIterator);
        check();
    }

    void push_front()
    {
        control.push_front(Value());
        test.push_front(&control.front());
        check();
    }

    void push_front(size_t n)
    {
        Test chain;
        for (size_t i = 0; i < n; ++i)
        {
            control.push_front(Value());
            chain.push_front(&control.front());
        }
        test.push_front(chain);
        ASSERT_TRUE(chain.empty());
        check();
    }

    void push_back()
    {
        control.push_back(Value());
        test.push_back(&control.back());
        check();
    }

    void push_back(size_t n)
    {
        Test chain;
        for (size_t i = 0; i < n; ++i)
        {
            control.push_back(Value());
            chain.push_back(&control.back());
        }
        test.push_back(chain);
        ASSERT_TRUE(chain.empty());
        check();
    }

    void pop_front()
    {
        ASSERT_EQ(test.pop_front(), &control.front());
        control.pop_front();
        check();
    }

    void pop_back()
    {
        ASSERT_EQ(test.pop_back(), &control.back());
        control.pop_back();
        check();
    }

    bool empty()
    {
        EXPECT_EQ(test.empty(), control.empty());
        return control.empty();
    }

    iterator begin() { return iterator(test.begin(), control.begin()); }

    iterator end() { return iterator(test.end(), control.end()); }
};

template <unsigned _CHAIN_COUNT, unsigned _CHAIN_ID> void testDoubleChain()
{
    std::cout << "testDoubleChain<" << _CHAIN_COUNT << ", " << _CHAIN_ID << ">()" << std::endl;

    TestSelfDoubleChainedPointers<_CHAIN_COUNT, _CHAIN_ID> test;

    test.insert(test.begin());
    test.erase(test.begin());
    ASSERT_TRUE(test.empty());
    test.insert(test.end());
    test.erase(test.begin());
    ASSERT_TRUE(test.empty());

    test.push_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());
    test.push_front();
    test.pop_back();
    ASSERT_TRUE(test.empty());

    test.push_back();
    test.push_front();
    test.remove(test.begin());
    test.push_front();
    test.insert(++test.begin());
    test.remove(--test.end());
    test.erase(test.begin());
    test.push_back();
    test.pop_front();
    test.pop_back();
    ASSERT_TRUE(test.empty());

    test.push_front(0);
    test.push_back(0);
    test.push_front(1);
    test.push_back(1);
    test.pop_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());

    test.push_back(1);
    test.push_front(0);
    test.push_back(0);
    test.pop_front();
    ASSERT_TRUE(test.empty());

    test.push_front(1);
    test.push_front(0);
    test.push_back(0);
    test.pop_back();
    ASSERT_TRUE(test.empty());

    test.push_back(2);
    test.pop_front();
    test.pop_back();
    ASSERT_TRUE(test.empty());

    test.push_front(2);
    test.pop_front();
    test.pop_back();
    ASSERT_TRUE(test.empty());

    test.push_front(1);
    test.push_back(2);
    test.push_front(1);
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());

    test.push_back(1);
    test.push_front(2);
    test.push_back(1);
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());

    test.push_front(2);
    test.push_back(1);
    test.push_front(3);
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());

    test.push_back(2);
    test.push_front(1);
    test.push_back(3);
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    test.pop_back();
    test.pop_front();
    ASSERT_TRUE(test.empty());
}

static void testDoubleChain()
{
    testDoubleChain<1u, 0u>();
    testDoubleChain<2u, 0u>();
    testDoubleChain<2u, 1u>();
}

struct TestDoubleChain2 : tredzone::MultiDoubleChainLink<TestDoubleChain2, 2>
{
    int v;
    TestDoubleChain2(int v) : v(v) {}
};

void testDoubleChain2()
{
    TestDoubleChain2::DoubleChain<0u> chain0;
    TestDoubleChain2::DoubleChain<1u> chain1;
    std::list<TestDoubleChain2> list;

    list.push_back(0);
    ASSERT_TRUE(chain0.empty());
    ASSERT_TRUE(chain1.empty());
    chain0.push_back(&list.front());
    ASSERT_TRUE(!chain0.empty());
    ASSERT_EQ(chain0.front(), &list.front());
    ASSERT_EQ(++chain0.begin(), chain0.end());
    ASSERT_TRUE(chain1.empty());
    chain1.push_back(&list.front());
    ASSERT_TRUE(!chain0.empty());
    ASSERT_EQ(chain0.front(), &list.front());
    ASSERT_EQ(++chain0.begin(), chain0.end());
    ASSERT_TRUE(!chain1.empty());
    ASSERT_EQ(chain1.front(), &list.front());
    ASSERT_EQ(++chain1.begin(), chain1.end());

    chain0.remove(&list.front());
    ASSERT_TRUE(chain0.empty());
    ASSERT_TRUE(!chain1.empty());
    ASSERT_EQ(chain1.front(), &list.front());
    ASSERT_EQ(++chain1.begin(), chain1.end());

    chain1.remove(&list.front());
    ASSERT_TRUE(chain0.empty());
    ASSERT_TRUE(chain1.empty());
}

TEST(MDoubleChain, doubleChain) { testDoubleChain(); }
TEST(MDoubleChain, doubleChain2) { testDoubleChain2(); }