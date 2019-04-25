/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file tcpserver.cpp
 * @brief tcp server basic sample
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "trz/connector/tcpconnector.hpp"

#include <iostream>

using namespace std;

constexpr size_t MaxReadPerLoop                 = 4;
constexpr size_t MaxWritePerLoop                = 4;
constexpr size_t MaxBytePerRead                 = 4096;
constexpr size_t MaxPendingIncomingConnection   = 10;

constexpr size_t SendBufferSize                 = 8192;
constexpr size_t ReceiveBufferSize              = 8192;

using MyNetwork = Network<MaxReadPerLoop, MaxWritePerLoop, MaxBytePerRead, MaxPendingIncomingConnection>;

//---- server-spawned process talking to connected client ----------------------

class ServerProcess : public TcpClient<MyNetwork, SendBufferSize, ReceiveBufferSize>
{
protected:

    void onNewMessageToRead(void) noexcept override {cout << "ServerProcess::onNewMessageToRead();" << endl; }

    void onConnectFailed(void) noexcept override { cout << "ServerProcess::onConnectFailed();" << endl; }

    void onConnectTimedOut(void) noexcept override { cout << "ServerProcess::onConnectTimedOut();" << endl; }

    void onConnectionLost(void) noexcept override { cout << "ServerProcess::onConnectionLost();" << endl; }

    void onConnect(void) noexcept override 
    {
        cout << "ServerProcess::onConnect();" << endl; 
        send("Welcome!");
    }

    void onDisconnect(void) noexcept override { cout << "ServerProcess::onDisconnect();" << endl; }

    void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        cout << "ServerProcess::onDataReceived(); --------------" << endl;
        cout.write(reinterpret_cast<const char *>(data), dataSize);
        cout << endl << "-----------------------------------------------" << endl;
        send("Echo: ");
        send(data, dataSize);
    }

    void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        cout << "ServerProcess::onOverflowDataReceived(); -------------" << endl;
        cout.write(reinterpret_cast<const char *>(data), dataSize);
        cout << endl << "------------------------------------------------------" << endl;
    }
};

//---- Server actor ------------------------------------------------------------

class MyServer : public TcpServer<MyNetwork, ServerProcess>
{
    using parent = TcpServer<MyNetwork, ServerProcess>;

public:

    // ctor
    MyServer(void) noexcept
    {
        registerListen(AF_INET, INADDR_ANY, 8080);
    }
    
protected:

    virtual void onServerProcessDestroy(const ActorId &serverProcessActorId) noexcept override 
    {
        cout << "Server::onServerProcessDestroy();" << endl
        << "    client managed by server process actor " << serverProcessActorId  << " disconnected," << endl
        << "    the actor will be destroyed" << endl; 
    }

    virtual void onNewConnection(const fd_t fd, const char *clientIp, const ActorId &serverProcessActorId) noexcept override
    {
        cout << "Server::onNewConnection();" << endl
        << "    client from " << clientIp << endl
        << "    connected on socket " << fd << endl
        << "    and manageable through actor " << serverProcessActorId << endl; 
    }

    void onListenFailed(void) noexcept override 
    {
        cout << "Server::onListenFailed();" << endl;
    }

    void onListenSucceed(void) noexcept override 
    {
        cout << "Server::onListenSucceed();" << endl;
    }

    void onAcceptFailed(void) noexcept override 
    {
        cout << "Server::onAcceptFailed();" << endl;
    }
    
    void onListenStopped(void) noexcept override 
    {
        cout << "Server::onListenStopped();" << endl;
    }
};

//---- MAIN ---------------------------------------------------------------------

int main()
{
    Engine::StartSequence   start_seq;

    start_seq.addActor<MyServer>(0);

    Engine engine(start_seq);

    cout << "Press enter to end the program " << endl;
    getchar();
    return 0;
}