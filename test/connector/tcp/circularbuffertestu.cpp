#include <iostream>
#include <sstream>
#include <string>
#include <tuple>

#include "trz/connector/tcpconnector.hpp"
#include "gtest/gtest.h"

using namespace tredzone;
using namespace tredzone::connector;
using namespace tredzone::connector::tcp;
using namespace std;

constexpr int Capacity = 10;

bool checkContent(CircularBuffer<Capacity>& buffer, const char* content) 
{
    ostringstream dataSs;
    dataSs.write(reinterpret_cast<char *>(get<0>(buffer.readData())), get<1>(buffer.readData()));
    const string data = dataSs.str();
    return (data.compare(content) == 0);
}

TEST(tcp_utest, CircularBuffer)
{
    bool writeSucceedFlag = false;

    CircularBuffer<Capacity> buffer;


    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test0test0"), 10);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"test0test0")); // [test0test0]

    buffer.clear();
    ASSERT_TRUE(checkContent(buffer,"")); // [__________]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test1"), 5);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"test1")); // [test1_____]

    buffer.shiftBuffer(4);
    ASSERT_TRUE(checkContent(buffer,"1")); // [____1_____]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test2"), 5);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"1test2")); // [1test2____]

    buffer.shiftBuffer(5);
    ASSERT_TRUE(checkContent(buffer,"2")); // [_________2]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test3"), 5);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"2test3")); // [test3____2]

    buffer.shiftBuffer(5);
    ASSERT_TRUE(checkContent(buffer,"3")); // [____3_____]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test04"), 6);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"3test04")); // [4___3test0]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("lol"), 3);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"3test04lol")); // [4lol3test0]

    buffer.shiftBuffer(7);
    ASSERT_TRUE(checkContent(buffer,"lol")); // [_lol______]
    buffer.shiftBuffer(3);
    ASSERT_TRUE(checkContent(buffer,"")); // [__________]

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test05"), 6);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"test05")); // [test05____]

    buffer.shiftBuffer(6);

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("testTooLongData"), 15);
    ASSERT_TRUE(!writeSucceedFlag);

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("test"), 4);
    ASSERT_TRUE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"test"));

    writeSucceedFlag = buffer.writeData(reinterpret_cast<const uint8_t *>("TooLongData"), 11);
    ASSERT_FALSE(writeSucceedFlag);
    ASSERT_TRUE(checkContent(buffer,"test"));
}
