/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file tcpclient.cpp
 * @brief basic tcp client
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include "trz/connector/tcpconnector.hpp"

#include <iostream>

using namespace std;

constexpr size_t MaxReadPerLoop                 = 4;
constexpr size_t MaxBytePerRead                 = 4096;
constexpr size_t MaxWritePerLoop                = 4;
constexpr size_t MaxPendingIncomingConnection   = 10;

constexpr size_t SendBufferSize                 = 8192;
constexpr size_t ReceiveBufferSize              = 8192;

using MyNetwork = Network<MaxReadPerLoop, MaxWritePerLoop, MaxBytePerRead, MaxPendingIncomingConnection>;

//---- client actor ------------------------------------------------------------

class MyClient : public TcpClient<MyNetwork, SendBufferSize, ReceiveBufferSize>
{
    public:
    MyClient() { registerConnect("127.0.0.1", AF_INET, 8080, 1, 0); }

    ~MyClient() {}

    protected:
    void onNewMessageToRead(void) noexcept override {cout << "Client::onNewMessageToRead();" << endl; }

    void onConnectFailed(void) noexcept override { cout << "Client::onConnectFailed();" << endl; }

    void onConnectTimedOut(void) noexcept override { cout << "Client::onConnectTimedOut();" << endl; }

    void onConnectionLost(void) noexcept override { cout << "Client::onConnectionLost();" << endl; }

    void onConnect(void) noexcept override { cout << "Client::onConnect();" << endl; }

    void onDisconnect(void) noexcept override { cout << "Client::onDisconnect();" << endl; }

    void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        cout << "Client::onDataReceived(); --------------" << endl;
        cout.write(reinterpret_cast<const char *>(data), dataSize);
        cout << endl << "----------------------------------------" << endl;
        if (string(reinterpret_cast<const char *>(data), dataSize).find("Echo") == string::npos)
            send("Hello!");
    }

    void onOverflowDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        cout << "Client::onOverflowDataReceived(); --------------" << endl;
        cout.write(reinterpret_cast<const char *>(data), dataSize);
        cout << endl << "------------------------------------------------" << endl;
    }
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    Engine::StartSequence startSequence;

    startSequence.addActor<MyClient>(1);

    Engine engine(startSequence);

    cout << "Press enter to end the program " << endl;
    getchar();
    
    return 0;
}