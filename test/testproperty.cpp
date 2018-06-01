/**
 * @file testproperty.cpp
 * @brief test graph property
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <limits>

#include "gtest/gtest.h"

#include "trz/engine/internal/property.h"

#include "testutil.h"

using namespace tredzone;

struct NoParameter
{
    inline NoParameter() {}
    inline NoParameter(const NoParameter &) {}
};

typedef Property<TestAllocator<void, TestMemory>> TestProperty;

template <class _Value> struct TestPropertyCheck
{
    void operator()(TestProperty &property, const char *name, const _Value &value, TestMemory &testMemory)
    {
        const TestProperty &const_property = property;
        property.setValue<_Value>(value);
        ASSERT_EQ(property.get_allocator().getMemory(), &testMemory);
        ASSERT_EQ(value, property.getValue<_Value>());
        ASSERT_EQ(value, const_property.getValue<_Value>());
        {
            _Value &abstractValue =
                dynamic_cast<PropertyValue<TestProperty::allocator_type, _Value> &>(property.getAbstractValue()).value;
            ASSERT_EQ(value, abstractValue);
        }
        {
            const _Value &const_abstractValue =
                dynamic_cast<const PropertyValue<TestProperty::allocator_type, _Value> &>(
                    const_property.getAbstractValue())
                    .value;
            ASSERT_EQ(value, const_abstractValue);
        }

        property.setNameAndValue<_Value>(name, value);
        ASSERT_EQ(property.get_allocator().getMemory(), &testMemory);
        ASSERT_EQ(std::string(property.getName()), name);
        ASSERT_EQ(value, property.getValue<_Value>());
        ASSERT_EQ(value, const_property.getValue<_Value>());
        {
            _Value &abstractValue =
                dynamic_cast<PropertyValue<TestProperty::allocator_type, _Value> &>(property.getAbstractValue()).value;
            ASSERT_EQ(value, abstractValue);
        }
        {
            const _Value &const_abstractValue =
                dynamic_cast<const PropertyValue<TestProperty::allocator_type, _Value> &>(
                    const_property.getAbstractValue())
                    .value;
            ASSERT_EQ(value, const_abstractValue);
        }
        ASSERT_NE(property.nullValue(), property.getAbstractValue());
        ASSERT_NE(property.nullValue(), const_property.getAbstractValue());
    }
};

template <> struct TestPropertyCheck<PropertyNullValue>
{
    void operator()(TestProperty &property, const char *name, const PropertyNullValue &value, TestMemory &testMemory)
    {
        property.setNameAndAbstractValue(name, value);
        ASSERT_EQ(property.get_allocator().getMemory(), &testMemory);
        ASSERT_EQ(std::string(property.getName()), name);
        ASSERT_EQ(value, property.getAbstractValue());
        ASSERT_EQ(property.nullValue(), property.getAbstractValue());
    }
};

template <class _Value>
void testPropertyCheck(const TestProperty &property, const char *name, const _Value &value, TestMemory &testMemory)
{
    size_t memoryInUse = testMemory.inUse;
    ASSERT_EQ(property.get_allocator().getMemory(), &testMemory);
    ASSERT_EQ(std::string(property.getName()), name);
    {
        TestProperty property2 = property;
        ASSERT_EQ(property2.get_allocator().getMemory(), &testMemory);
        ASSERT_EQ(std::string(property2.getName()), name);
    }
    {
        testMemory.badAlloc = 0;

        TestMemory testMemory2;
        TestProperty::allocator_type allocator2(testMemory2);
        TestProperty property2(allocator2);
        property2 = property;
        ASSERT_EQ(property2.get_allocator().getMemory(), &testMemory2);
        ASSERT_EQ(std::string(property2.getName()), name);

        testMemory.badAlloc = std::numeric_limits<size_t>::max();
    }
    {
        testMemory.badAlloc = 0;

        TestMemory testMemory2;
        TestProperty::allocator_type allocator2(testMemory2);
        TestProperty property2(allocator2);
        property2.setName(property.getName()).setAbstractValue(property.getAbstractValue());
        ASSERT_EQ(property2.get_allocator().getMemory(), &testMemory2);
        ASSERT_EQ(std::string(property2.getName()), name);

        property2.setNameAndAbstractValue("", TestProperty::nullValue());
        ASSERT_EQ(property2.get_allocator().getMemory(), &testMemory2);
        ASSERT_EQ(std::string(property2.getName()), "");
        ASSERT_EQ(TestProperty::nullValue(), property2.getAbstractValue());
        ASSERT_EQ(property2.getAbstractValue(), TestProperty::nullValue());

        property2.setName(property.getName());
        ASSERT_EQ(std::string(property2.getName()), name);

        property2.setNameAndAbstractValue(TestProperty::string_type(), TestProperty::nullValue());
        ASSERT_EQ(property2.get_allocator().getMemory(), &testMemory2);
        ASSERT_EQ(std::string(property2.getName()), "");
        ASSERT_EQ(TestProperty::nullValue(), property2.getAbstractValue());
        ASSERT_EQ(property2.getAbstractValue(), TestProperty::nullValue());

        TestPropertyCheck<_Value>()(property2, name, value, testMemory2);
        testMemory.badAlloc = std::numeric_limits<size_t>::max();
    }
    ASSERT_EQ(testMemory.inUse, memoryInUse);
}

void testProperty()
{
    TestMemory testMemory;
    TestAllocator<void, TestMemory> allocator(testMemory);
    testPropertyCheck(TestProperty(allocator), "", TestProperty::nullValue(), testMemory);
    testPropertyCheck(TestProperty(allocator).setName("Test"), "Test", TestProperty::nullValue(), testMemory);
    testPropertyCheck(TestProperty(allocator).setValue<int>(1), "", (int)1, testMemory);
    testPropertyCheck(TestProperty(allocator).setName("Test").setValue<int>(1), "Test", (int)1, testMemory);
}

struct TestPropertyExceptionsValue
{
    TestPropertyExceptionsValue(bool throwException)
    {
        if (throwException)
        {
            throw std::bad_alloc();
        }
    }
    TestPropertyExceptionsValue(const TestPropertyExceptionsValue &) { throw std::bad_alloc(); }
};

std::ostream &operator<<(std::ostream &left, const TestPropertyExceptionsValue &) { return left; }

void testPropertyBadAlloc()
{
    TestMemory testMemory;
    TestAllocator<void, TestMemory> allocator(testMemory);
    TestProperty property(allocator);
    testMemory.badAlloc = 0;
    property.setAbstractValue(property.nullValue());
    testMemory.badAlloc = std::numeric_limits<size_t>::max();
    property.setName("XXX");
    size_t szIntValue = testMemory.inUse;
    property.setValue<int>(1);
    szIntValue = testMemory.inUse - szIntValue;
    testMemory.badAlloc = 0;

    ASSERT_THROW(property.setName("Test"), std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(1, property.getValue<int>());

    ASSERT_THROW(property.setValue<char>((char)0), std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(1, property.getValue<int>());

    ASSERT_THROW(property.setNameAndValue<char>("Test", (char)0), std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(1, property.getValue<int>());

    ASSERT_THROW(property.setNameAndAbstractValue("Test", TestProperty::nullValue()), std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(1, property.getValue<int>());

    testMemory.badAlloc = std::numeric_limits<size_t>::max();
    property.setValue<int>(2);
    ASSERT_THROW(property.setValue<TestPropertyExceptionsValue>(true), std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(2, property.getValue<int>());

    {
        PropertyValue<TestProperty::allocator_type, TestPropertyExceptionsValue> value(property, false);
        ASSERT_THROW(property.setAbstractValue(value), std::bad_alloc);
        ASSERT_EQ("XXX", std::string(property.getName()));
        ASSERT_EQ(2, property.getValue<int>());
    }
    {
        PropertyValue<TestProperty::allocator_type, TestPropertyExceptionsValue> value(property, false);
        ASSERT_THROW(property.setNameAndValue<TestPropertyExceptionsValue>("Test", true), std::bad_alloc);
        ASSERT_EQ("XXX", std::string(property.getName()));
        ASSERT_EQ(2, property.getValue<int>());
    }
    {
        PropertyValue<TestProperty::allocator_type, TestPropertyExceptionsValue> value(property, false);
        ASSERT_THROW(property.setNameAndAbstractValue("Test", value), std::bad_alloc);
        ASSERT_EQ("XXX", std::string(property.getName()));
        ASSERT_EQ(2, property.getValue<int>());
    }

    testMemory.badAlloc = testMemory.inUse + szIntValue;
    ASSERT_THROW(
        property.setNameAndValue<int>(
            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890", 3),
        std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(2, property.getValue<int>());

    testMemory.badAlloc = testMemory.inUse + sizeof(TestProperty::string_type) + 1;
    ASSERT_THROW(
        property.setNameAndValue<TestProperty::string_type>(
            "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890",
            "test"),
        std::bad_alloc);
    ASSERT_EQ("XXX", std::string(property.getName()));
    ASSERT_EQ(2, property.getValue<int>());
}

template <class _Iterator, class _PropertyCollection>
void testPropertyCollectionIterator(_PropertyCollection &property, TestMemory *testMemory, size_t level)
{
    ASSERT_FALSE(property.empty());
    std::ostringstream ostr;
    ostr << level << '.' << std::ends;
    _Iterator i = property.begin();
    _Iterator j = i;
    ASSERT_NE(i, property.end());
    ASSERT_EQ(i->get_allocator().getMemory(), testMemory);
    ASSERT_EQ(TestProperty::string_type(i->getName()), TestProperty::string_type() + ostr.str().c_str() + '0');
    const TestProperty::Collection &prop2 = *i;
    ASSERT_EQ(0, prop2.getValue<int>());
    ASSERT_EQ(property.find(i->getName()), i);
    ASSERT_EQ(property.find(i, i->getName()), i);
    ASSERT_EQ(property.find(++j, i->getName()), property.end());
    if (level % 3 > 0)
    {
        ASSERT_NE(++i, property.end());
        ASSERT_EQ(i->get_allocator().getMemory(), testMemory);
        ASSERT_EQ(TestProperty::string_type(i->getName()), TestProperty::string_type() + ostr.str().c_str() + '1');
        const TestProperty::Collection &prop3 = *i;
        ASSERT_EQ(1, prop3.getValue<int>());
        ASSERT_EQ(property.find(i->getName()), i);
        ASSERT_EQ(property.find(i, i->getName()), i);
        ASSERT_EQ(property.find(++j, i->getName()), property.end());
        if (level % 3 > 1)
        {
            ASSERT_NE(++i, property.end());
            ASSERT_EQ(i->get_allocator().getMemory(), testMemory);
            ASSERT_EQ(TestProperty::string_type(i->getName()), TestProperty::string_type() + ostr.str().c_str() + '2');
            const TestProperty::Collection &prop4 = *i;
            ASSERT_EQ(2, prop4.getValue<int>());
            ASSERT_EQ(property.find(std::string(i->getName())), i);
            ASSERT_EQ(property.find(i, std::string(i->getName())), i);
            ASSERT_EQ(property.find(++j, std::string(i->getName())), property.end());
        }
    }
    ASSERT_EQ(++i, property.end());
    ASSERT_EQ(property.find(""), property.end());
}

template <size_t DEPTH>
void testPropertyCollection(TestProperty::Collection &property, TestMemory *testMemory, size_t level = 0)
{
    ASSERT_TRUE(level < DEPTH);
    const TestProperty::Collection &const_property = property;
    ASSERT_EQ(property.get_allocator().getMemory(), testMemory);
    ASSERT_EQ(const_property.get_allocator().getMemory(), testMemory);
    testPropertyCollectionIterator<TestProperty::Collection::iterator>(property, testMemory, level);
    testPropertyCollectionIterator<TestProperty::Collection::const_iterator>(const_property, testMemory, level);
    if (++level < DEPTH)
    {
        for (TestProperty::Collection::iterator i = property.begin(), endi = property.end(); i != endi; ++i)
        {
            testPropertyCollection<DEPTH>(*i, testMemory, level);
        }
    }
}

template <size_t DEPTH>
TestProperty::Collection getTestPropertyCollection2(TestProperty::Collection::allocator_type allocator, size_t level)
{
    TestProperty::Collection ret(allocator);
    std::basic_ostringstream<TestProperty::string_type::value_type, TestProperty::string_type::traits_type,
                             TestProperty::string_type::allocator_type>
        ostr(ret.getName());
    ostr << level << '.' << std::ends;
    const TestProperty::Collection &const_ret = ret;
    EXPECT_TRUE(ret.empty());
    EXPECT_EQ(ret.begin(), ret.end());
    EXPECT_EQ(const_ret.begin(), const_ret.end());
    ret.insert()->setNameAndValue<int>(TestProperty::string_type(ret.get_allocator()) + ostr.str().c_str() + '0', 0);
    switch (level % 3)
    {
    case 0:
        break;
    case 2:
        ret.insert(++ret.begin())
            ->setNameAndValue<int>(TestProperty::string_type(ret.get_allocator()) + ostr.str().c_str() + '2', 2);
		// fallthrough
		// Don't remove the previous line to disable gcc 7 warning
    case 1:
        ret.insert(++ret.begin())
            ->setNameAndValue<int>(TestProperty::string_type(ret.get_allocator()) + ostr.str().c_str() + '1', 1);
        break;
    default:
        ADD_FAILURE() << "This cannot happen";
    }
    return ret;
}

template <size_t DEPTH> TestProperty::Collection getTestPropertyCollection(TestMemory &testMemory, size_t level = 0)
{
    EXPECT_TRUE(level < DEPTH);
    TestProperty::Collection::allocator_type allocator(testMemory);
    TestProperty::Collection ret(allocator);
    ret = getTestPropertyCollection2<DEPTH>(allocator, level);
    if (++level < DEPTH)
    {
        for (TestProperty::Collection::iterator i = ret.begin(), endi = ret.end(); i != endi; ++i)
        {
            TestProperty property = *i;
            *i = getTestPropertyCollection<DEPTH>(testMemory, level);
            *i = property;
        }
    }
    testPropertyCollection<DEPTH>(ret, &testMemory, level - 1);
    return ret;
}

inline std::ostream &operator<<(std::ostream &left, const Property<TestAllocator<void, TestMemory>> &right)
{
    return right.toOStream(left);
}

inline std::ostream &operator<<(std::ostream &left, const Property<TestAllocator<void, TestMemory>>::Collection &right)
{
    return right.toOStream(left);
}

void testPropertyCollection()
{
    static const size_t DEPTH = 4;
    TestMemory testMemory;
    TestProperty::Collection propertyCollection = getTestPropertyCollection<DEPTH>(testMemory);
    {
        TestProperty::Collection propertyCollection2 = propertyCollection;
        testPropertyCollection<DEPTH>(propertyCollection2, &testMemory);
    }
    {
        TestMemory testMemory2;
        TestProperty::Collection::allocator_type allocator(testMemory2);
        TestProperty::Collection propertyCollection2(allocator);
        propertyCollection2 = propertyCollection;
        testPropertyCollection<DEPTH>(propertyCollection2, &testMemory2);
    }
    {
        ASSERT_EQ(propertyCollection.erase(propertyCollection.begin()), propertyCollection.end());
        ASSERT_TRUE(propertyCollection.empty());
        propertyCollection = getTestPropertyCollection<DEPTH>(testMemory);
        TestProperty::Collection &leave = *propertyCollection.begin()->begin();
        TestProperty::Collection::iterator i = leave.erase(++leave.begin());
        ASSERT_EQ(i, ++leave.begin());
        i = leave.erase(leave.begin());
        ASSERT_EQ(i, leave.begin());
    }
    {
        TestMemory testMemory2;
        TestProperty::Collection::allocator_type allocator(testMemory2);
        TestProperty::Collection propertyCollection2(allocator);
        size_t sz = testMemory2.inUse;
        propertyCollection2 = getTestPropertyCollection<DEPTH>(testMemory);
        ASSERT_TRUE(testMemory2.inUse > sz);
        ASSERT_FALSE(propertyCollection2.empty());
        propertyCollection2.clear();
        ASSERT_EQ(testMemory2.inUse, sz);
        ASSERT_TRUE(propertyCollection2.empty());
    }
    {
        std::basic_ostringstream<char, std::char_traits<char>, TestAllocator<char, TestMemory>> ostm;
        TestProperty property;
        property.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "");
        ostm.seekp(0);
        property.setNameAndValue<int>("Test", 0);
        ostm << property;
        ASSERT_EQ(ostm.str(), "Test=0");
        ostm.seekp(0);
        TestProperty::Collection propertyCollection = property;
        propertyCollection.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "{Test=0}");
        ostm.seekp(0);
        propertyCollection.insert();
        propertyCollection.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "{Test=0:{}}");
        ostm.seekp(0);
        propertyCollection.insert();
        propertyCollection.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "{Test=0:{,}}");
        ostm.seekp(0);
        propertyCollection.begin()->insert();
        propertyCollection.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "{Test=0:{{},}}");
        ostm.seekp(0);
        propertyCollection.begin()->setName("level1");
        propertyCollection.toOStream(ostm);
        ASSERT_EQ(ostm.str(), "{Test=0:{level1:{},}}");
        ostm.seekp(0);
        propertyCollection.begin()->begin()->setNameAndValue<bool>("level2", false);
        ostm << propertyCollection;
        ASSERT_EQ(ostm.str(), "{Test=0:{level1:{level2=false},}}");

        std::basic_ostringstream<char, std::char_traits<char>, TestAllocator<char, TestMemory>> ostm2;
        propertyCollection.setName("");
        ostm2 << propertyCollection;
        ASSERT_EQ(ostm2.str(), "{=0:{level1:{level2=false},}}");
    }
}

struct TestPropertyCollectionBadAlloc
{
    TestPropertyCollectionBadAlloc(const NoParameter &) {}
    TestPropertyCollectionBadAlloc(const TestPropertyCollectionBadAlloc &) { throw std::bad_alloc(); }
};

std::ostream &operator<<(std::ostream &left, const TestPropertyCollectionBadAlloc &) { return left; }

void testPropertyCollectionBadAlloc()
{
    TestMemory testMemory;
    TestProperty::allocator_type allocator(testMemory);
    {
        TestProperty::Collection propertyCollection(allocator);
        testMemory.badAlloc = testMemory.inUse + sizeof(TestProperty::Collection);

        ASSERT_THROW(propertyCollection.insert(), std::bad_alloc);

        ASSERT_TRUE(propertyCollection.empty());
        testMemory.badAlloc = std::numeric_limits<size_t>::max();
        propertyCollection.insert();
        TestProperty::Collection propertyCollection2(allocator);
        testMemory.badAlloc = testMemory.inUse + sizeof(TestProperty::Collection);

        ASSERT_THROW(propertyCollection2 = propertyCollection, std::bad_alloc);

        testMemory.badAlloc = std::numeric_limits<size_t>::max();
    }
    {
        TestProperty::Collection propertyCollection(allocator);
        propertyCollection.setNameAndValue<int>("Old", 1);
        propertyCollection.insert()->setNameAndValue<int>("Old.1", 11);
        TestProperty::Collection propertyCollection2(allocator);
        propertyCollection2.setNameAndValue<TestPropertyCollectionBadAlloc>("New", NoParameter());
        propertyCollection2.insert()->setNameAndValue<int>("New.2", 2);

        ASSERT_THROW(propertyCollection = propertyCollection2, std::bad_alloc);

        ASSERT_EQ(std::string(propertyCollection.getName()), "Old");
        ASSERT_EQ(propertyCollection.getValue<int>(), 1);
        ASSERT_EQ(std::string(propertyCollection.begin()->getName()), "Old.1");
        ASSERT_EQ(propertyCollection.begin()->getValue<int>(), 11);
        ASSERT_TRUE(propertyCollection.begin()->empty());
    }
}

void testPropertyBadCast()
{
    TestMemory testMemory;
    TestProperty::allocator_type allocator(testMemory);
    TestProperty property(allocator);
    const TestProperty &const_property = property;
    ASSERT_EQ(TestProperty::nullValue(), property.getAbstractValue());
    ASSERT_EQ(property.getAbstractValue(), TestProperty::nullValue());
    property.setValue<int>(1);
    ASSERT_EQ(1, property.getValue<int>());

    ASSERT_THROW(property.getValue<char>(), std::bad_cast);
    ASSERT_THROW(const_property.getValue<char>(), std::bad_cast);

    property.getValue<int>() = 2;
    ASSERT_EQ(2, property.getValue<int>());
}

TEST(Property, property) { testProperty(); }
TEST(Property, badAlloc) { testPropertyBadAlloc(); }
TEST(Property, badCast) { testPropertyBadCast(); }
TEST(Property, collection) { testPropertyCollection(); }
TEST(Property, collectionBadAlloc) { testPropertyCollectionBadAlloc(); }
