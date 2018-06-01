/**
 * @file testparallel.cpp
 * @brief test parallel memory access
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "gtest/gtest.h"

#include <iostream>

#include "trz/engine/internal/parallel.h"

struct TestNodesHandler
{
    static const int MAX_SIZE = 10;
    static TestNodesHandler *instance;

    typedef unsigned char NodeId;

    struct Shared
    {
        int value;
        Shared() : value(0) {}
        bool operator==(const Shared &other) const { return value == other.value; }
        Shared &operator=(int pvalue)
        {
            value = pvalue;
            return *this;
        }
    };

    struct SharedHandleData
    {
        enum ReaderCASEnum
        {
            READER_CAS_IDLE = 0,
            READER_CAS_ON,
            READER_CAS_OFF
        };
        bool isWriteLocked;
        bool isReaderActive;
        sig_atomic_t readerCAS;
        bool isWriterActive;
        Shared shared;
        SharedHandleData()
            : isWriteLocked(false), isReaderActive(false), readerCAS(READER_CAS_IDLE), isWriterActive(false)
        {
        }
    };

    struct SharedHandleControlData : SharedHandleData
    {
        bool readCallToggle;
        bool writeCallToggle;
        bool writeFailedCallToggle;
        Shared readValue;
        SharedHandleControlData() : readCallToggle(false), writeCallToggle(false), writeFailedCallToggle(false) {}
    };

    struct SharedHandle : SharedHandleData
    {
        SharedHandleControlData controlData;
        bool read()
        {
            EXPECT_TRUE(controlData.readCallToggle);
            EXPECT_EQ(controlData.readValue, shared);
            controlData.readCallToggle = false;
            return true;
        }
        bool write()
        {
            EXPECT_TRUE(controlData.writeCallToggle);
            controlData.writeCallToggle = false;
            return true;
        }
        void writeFailed()
        {
            ASSERT_TRUE(controlData.writeFailedCallToggle);
            controlData.writeFailedCallToggle = false;
        }
        void control()
        {
            ASSERT_EQ(isWriteLocked, controlData.isWriteLocked);
            ASSERT_EQ(isReaderActive, controlData.isReaderActive);
            ASSERT_EQ(readerCAS, controlData.readerCAS);
            ASSERT_EQ(isWriterActive, controlData.isWriterActive);
            ASSERT_TRUE(!controlData.readCallToggle);
            ;
            ASSERT_TRUE(!controlData.writeCallToggle);
            ASSERT_TRUE(!controlData.writeFailedCallToggle);
        }
        bool getIsWriterActive() { return isWriterActive; }
        void setIsWriterActive(bool pisWriterActive) { isWriterActive = pisWriterActive; }
        bool getIsReaderActive() { return isReaderActive; }
        void setIsReaderActive(bool pisReaderActive) { isReaderActive = pisReaderActive; }
        sig_atomic_t &getReferenceToReaderCAS() { return readerCAS; }
        bool getIsWriteLocked() { return isWriteLocked; }
        void setIsWriteLocked(bool pisWriteLocked) { isWriteLocked = pisWriteLocked; }
        Shared &getReferenceToShared() { return shared; }
    };

    typedef SharedHandle ReaderSharedHandle;
    typedef SharedHandle WriterSharedHandle;

    struct NodeHandleData
    {
        bool isActive;
#ifndef NDEBUG
        bool debugSynchronizeWriteFailedOperatorCalled;
#endif
        NodeHandleData()
            : isActive(false)
#ifndef NDEBUG
              ,
              debugSynchronizeWriteFailedOperatorCalled(false)
#endif
        {
        }
    };

    struct NodeHandle : NodeHandleData
    {
        TestNodesHandler &testNodesHandler;
        const NodeId id;
        NodeHandleData controlData;
        NodeHandle(TestNodesHandler &ptestNodesHandler, NodeId pid) : testNodesHandler(ptestNodesHandler), id(pid)
        {
            EXPECT_TRUE(id < testNodesHandler.size);
        }
        WriterSharedHandle &getWriterSharedHandle(NodeId readerNodeId)
        {
            return testNodesHandler.sharedHandle[id * MAX_SIZE + readerNodeId];
        }
        ReaderSharedHandle &getReaderSharedHandle(NodeId writerNodeId)
        {
            return testNodesHandler.sharedHandle[writerNodeId * MAX_SIZE + id];
        }
        template <class _Operator> void foreachRead(NodeId readerNodeId, _Operator &op)
        {
            ASSERT_EQ(readerNodeId, id);
            for (int i = 0; i < id; ++i)
            {
                op(testNodesHandler.sharedHandle[i * MAX_SIZE + id], (NodeId)i);
            }
            for (int i = id + 1; i < (int)testNodesHandler.size; ++i)
            {
                op(testNodesHandler.sharedHandle[i * MAX_SIZE + id], (NodeId)i);
            }
        }
        template <class _Operator> void foreachWrite(NodeId writerNodeId, _Operator &op)
        {
            ASSERT_EQ(writerNodeId, id);
            for (int i = 0; i < id; ++i)
            {
                op(testNodesHandler.sharedHandle[id * MAX_SIZE + i], (NodeId)i);
            }
            for (int i = id + 1; i < (int)testNodesHandler.size; ++i)
            {
                op(testNodesHandler.sharedHandle[id * MAX_SIZE + i], (NodeId)i);
            }
        }
        void control()
        {
            ASSERT_EQ(NodeHandleData::isActive, controlData.isActive);
#ifndef NDEBUG
            ASSERT_EQ(debugSynchronizeWriteFailedOperatorCalled, controlData.debugSynchronizeWriteFailedOperatorCalled);
#endif
        }
    };

    typedef std::list<NodeHandle> NodeHandleList;

    TestNodesHandler(size_t psize) : size(psize)
    {
        EXPECT_TRUE(size <= (size_t)MAX_SIZE);
        for (int i = 0; i < (int)size; ++i)
        {
            nodeHandleList.push_back(NodeHandle(*this, (NodeId)i));
        }
        EXPECT_EQ(0, instance);
        instance = this;
    }
    ~TestNodesHandler()
    {
        EXPECT_EQ(instance, this);
        instance = 0;
    }
    void control()
    {
        for (NodeHandleList::iterator i = nodeHandleList.begin(), endi = nodeHandleList.end(); i != endi; ++i)
        {
            i->control();
        }
        for (int i = 0; i < MAX_SIZE; ++i)
        {
            for (int j = 0; j < MAX_SIZE; ++j)
            {
                sharedHandle[i * MAX_SIZE + j].control();
            }
        }
    }
    NodeHandle &getNodeHandle(NodeId nodeId)
    {
        NodeHandleList::iterator i = nodeHandleList.begin(), endi = nodeHandleList.end();
        for (; i != endi && i->id != nodeId; ++i)
        {
        }
        EXPECT_TRUE(i != endi);
        EXPECT_EQ(i->id, nodeId);
        return *i;
    }
    bool isNodeActive(NodeId nodeId)
    {
        EXPECT_TRUE(nodeId < size);
        return getNodeHandle(nodeId).isActive;
    }
    bool activateNode(NodeId nodeId)
    {
        EXPECT_TRUE(nodeId < size);
        if (!getNodeHandle(nodeId).isActive)
        {
            return getNodeHandle(nodeId).isActive = true;
        }
        return false;
    }
    void deactivateNode(NodeId nodeId)
    {
        ASSERT_TRUE(nodeId < size);
        ASSERT_TRUE(getNodeHandle(nodeId).isActive);
        getNodeHandle(nodeId).isActive = false;
    }
    SharedHandle &getSharedHandle(NodeId writerNodeId, NodeId readerNodeId)
    {
        return sharedHandle[writerNodeId * MAX_SIZE + readerNodeId];
    }

    const size_t size;
    NodeHandleList nodeHandleList;
    SharedHandle sharedHandle[MAX_SIZE * MAX_SIZE];
};

TestNodesHandler *TestNodesHandler::instance = 0;

typedef tredzone::Parallel<TestNodesHandler> TestParallel;

struct TestParallelNode : TestParallel::Node
{
    TestParallelNode(TestParallel &testParallel, TestParallel::NodeId nodeId) : TestParallel::Node(testParallel, nodeId)
    {
        {
            TestNodesHandler::NodeHandleList::iterator i, endi = TestNodesHandler::instance->nodeHandleList.end();
            for (i = TestNodesHandler::instance->nodeHandleList.begin(); i != endi && i->id != id; ++i)
            {
            }
            EXPECT_TRUE(i != endi);
            EXPECT_FALSE(i->controlData.isActive);
            i->controlData.isActive = true;
        }
        for (int i = 0; i < (int)TestNodesHandler::instance->size; ++i)
        {
            if (i != id)
            {
                TestNodesHandler::instance->sharedHandle[i * TestNodesHandler::MAX_SIZE + id]
                    .controlData.isReaderActive = true;
                TestNodesHandler::instance->sharedHandle[i * TestNodesHandler::MAX_SIZE + id].controlData.readerCAS =
                    TestNodesHandler::SharedHandleData::READER_CAS_ON;
                TestNodesHandler::instance->sharedHandle[id * TestNodesHandler::MAX_SIZE + i]
                    .controlData.isWriterActive = true;
            }
        }
    }
    ~TestParallelNode()
    {
        {
            TestNodesHandler::NodeHandleList::iterator i, endi = TestNodesHandler::instance->nodeHandleList.end();
            for (i = TestNodesHandler::instance->nodeHandleList.begin(); i != endi && i->id != id; ++i)
            {
            }
            EXPECT_TRUE(i != endi);
            EXPECT_TRUE(i->controlData.isActive);
            i->controlData.isActive = false;
        }
        for (int i = 0; i < (int)TestNodesHandler::instance->size; ++i)
        {
            if (i != id)
            {
                TestNodesHandler::instance->sharedHandle[i * TestNodesHandler::MAX_SIZE + id]
                    .controlData.isReaderActive = false;
                TestNodesHandler::instance->sharedHandle[i * TestNodesHandler::MAX_SIZE + id].controlData.readerCAS =
                    TestNodesHandler::SharedHandleData::READER_CAS_IDLE;
                TestNodesHandler::instance->sharedHandle[id * TestNodesHandler::MAX_SIZE + i]
                    .controlData.isWriterActive = false;
            }
        }
    }
};

void testInit(TestParallel &testParallel)
{
    TestNodesHandler::instance->control();

    {
        TestParallelNode testNode(testParallel, 0);

        TestNodesHandler::instance->control();
    }

    TestNodesHandler::instance->control();
}

static void testInit()
{
    TestParallel testParallel(2u);
    testInit(testParallel);
}

void testWrite(TestParallel &testParallel)
{
    TestNodesHandler::instance->control();

    {
        TestParallelNode testNode(testParallel, 0);

        ASSERT_EQ(0, testNode.id);

        TestNodesHandler::instance->control();

        testNode.synchronize();

        TestNodesHandler::instance->control();

        ASSERT_EQ(0, testNode.getReferenceToWriterShared(1).value);
        testNode.getReferenceToWriterShared(1) = 11;
        testNode.setWriteSignal(1, false);

        testNode.synchronize();

        TestNodesHandler::instance->control();

        testNode.setWriteSignal(1, true);
        TestNodesHandler::instance->getSharedHandle(0, 1).controlData.writeFailedCallToggle = true;

        testNode.synchronize();

        TestNodesHandler::instance->control();
    }

    TestNodesHandler::instance->control();
}

void testWrite()
{
    TestParallel testParallel(2u);
    testWrite(testParallel);
}

void testRead(TestParallelNode &writerNode, TestParallelNode &readerNode, int value)
{
    writerNode.getReferenceToWriterShared(readerNode.id).value = value;
    writerNode.setWriteSignal(readerNode.id, true);
    TestNodesHandler::instance->getSharedHandle(writerNode.id, readerNode.id).controlData.writeCallToggle = true;
    TestNodesHandler::instance->getSharedHandle(writerNode.id, readerNode.id).controlData.isWriteLocked = true;

    writerNode.synchronize();

    TestNodesHandler::instance->control();

    TestNodesHandler::instance->getSharedHandle(writerNode.id, readerNode.id).controlData.readValue = value;
    TestNodesHandler::instance->getSharedHandle(writerNode.id, readerNode.id).controlData.readCallToggle = true;
    TestNodesHandler::instance->getSharedHandle(writerNode.id, readerNode.id).controlData.isWriteLocked = false;

    readerNode.synchronize();

    TestNodesHandler::instance->control();
}

void testRead(TestParallel &testParallel)
{
    TestNodesHandler::instance->control();

    {
        TestParallelNode testNodeA(testParallel, 0);

        ASSERT_EQ(0, testNodeA.id);

        TestNodesHandler::instance->control();

        testNodeA.synchronize();

        TestNodesHandler::instance->control();

        TestParallelNode testNodeB(testParallel, 1);

        ASSERT_EQ(1, testNodeB.id);

        TestNodesHandler::instance->control();

        testNodeB.synchronize();

        TestNodesHandler::instance->control();

        testRead(testNodeA, testNodeB, 11);
        testRead(testNodeB, testNodeA, 22);
        testRead(testNodeB, testNodeA, 33);
    }

    TestNodesHandler::instance->control();
}

void testRead()
{
    TestParallel testParallel(2u);
    testRead(testParallel);
}

void testWriteFailed(TestParallel &testParallel)
{
    TestNodesHandler::instance->control();

    {
        TestParallelNode testNodeA(testParallel, 0);

        ASSERT_EQ(0, testNodeA.id);

        TestNodesHandler::instance->control();

        testNodeA.synchronize();

        {
            TestParallelNode testNodeB(testParallel, 1);

            ASSERT_EQ(1, testNodeB.id);

            TestNodesHandler::instance->control();

            testNodeB.synchronize();

            TestNodesHandler::instance->control();

            testRead(testNodeA, testNodeB, 11);

            testNodeA.getReferenceToWriterShared(testNodeB.id).value = 22;
            testNodeA.setWriteSignal(testNodeB.id, true);
            TestNodesHandler::instance->getSharedHandle(testNodeA.id, testNodeB.id).controlData.writeCallToggle = true;
            TestNodesHandler::instance->getSharedHandle(testNodeA.id, testNodeB.id).controlData.isWriteLocked = true;

            testNodeA.synchronize();

            TestNodesHandler::instance->control();
        }

#ifndef NDEBUG
        TestNodesHandler::instance->getNodeHandle(testNodeA.id).controlData.debugSynchronizeWriteFailedOperatorCalled =
            true;
#endif
        TestNodesHandler::instance->getSharedHandle(0, 1).controlData.writeFailedCallToggle = true;
        TestNodesHandler::instance->getSharedHandle(0, 1).controlData.isWriteLocked = false;

        testNodeA.synchronize();

        TestNodesHandler::instance->control();

#ifndef NDEBUG
        TestNodesHandler::instance->getNodeHandle(testNodeA.id).controlData.debugSynchronizeWriteFailedOperatorCalled =
            false;
#endif

        testNodeA.synchronize();

        TestNodesHandler::instance->control();
    }

    TestNodesHandler::instance->control();
}

void testWriteFailed()
{
    TestParallel testParallel(2u);
    testWriteFailed(testParallel);
}

void testAll()
{
    TestParallel testParallel(2u);
    testInit(testParallel);
    testWrite(testParallel);
    testRead(testParallel);
    testWriteFailed(testParallel);
}

TEST(Parallel, init) { testInit(); }
TEST(Parallel, write) { testWrite(); }
TEST(Parallel, read) { testRead(); }
TEST(Parallel, writeFailed) { testWriteFailed(); }
TEST(Parallel, all) { testAll(); }