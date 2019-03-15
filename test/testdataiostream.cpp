/**
 * @file testdataiostream.cpp
 * @brief test DataInput/OutputStream
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <sstream>

#include "trz/engine/internal/datastream.h"

using namespace std;
using namespace tredzone;

/* dump binary file with:

    od -A x <input_file>
*/
    
struct TestStruct
{
    TestStruct(const uint32_t &ui32 = 0, const string &s = "", const uint64_t &ui64 = 0, const bool &b = false, const int16_t &i16 = 0, const int32_t &i32 = 0)
        : m_ui32(ui32), m_s(s), m_ui64(ui64), m_b(b), m_i16(i16), m_i32(i32)
    {
    }
    
    TestStruct(DataInputStream<> &dis)
        : m_ui32(dis.Read32()), m_s(dis.ReadString()), m_ui64(dis.Read64()), m_b(dis.ReadBool()), m_i16(dis.Read16()), m_i32(dis.Read32())
    {
    }
    
    uint32_t        m_ui32;
    string          m_s;
    uint64_t        m_ui64;
    bool            m_b;
    int16_t         m_i16;
    int32_t         m_i32;
};

// overloads to de/serialize struct in one go
DataOutputStream<>&	operator<<(DataOutputStream<> &dos, const TestStruct &o)
{
    return dos << o.m_ui32 << o.m_s << o.m_ui64 << o.m_b << o.m_i16 << o.m_i32;
}

DataInputStream<>&	operator>>(DataInputStream<> &dis, TestStruct &o)
{
    return dis >> o.m_ui32 >> o.m_s >> o.m_ui64 >> o.m_b >> o.m_i16 >> o.m_i32;
}

static
void testInit()
{
    const TestStruct    istruct(0x01020304, "le string dans la raie", 0xffffffffffffff64ull, true, 0x0016u);
    
    // serialize
    ostringstream       oss;    
    DataOutputStream<>  dos(oss);
    
    dos << istruct;
    
    // deserialize
    istringstream       iss(oss.str());
    DataInputStream<>   dis(iss);
    
    TestStruct      ostruct(dis);
    
    // compare
    EXPECT_EQ(istruct.m_ui32, ostruct.m_ui32);
    EXPECT_EQ(istruct.m_s, ostruct.m_s);
    EXPECT_EQ(istruct.m_ui64, ostruct.m_ui64);
    EXPECT_EQ(istruct.m_b, ostruct.m_b);
    EXPECT_EQ(istruct.m_i16, ostruct.m_i16);
    EXPECT_EQ(istruct.m_i32, ostruct.m_i32);
}

TEST(DataStream, init) { testInit(); }
