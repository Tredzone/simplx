#include "trz/connector/httpconnector.hpp"
#include "simplx.h"

using namespace tredzone;
using namespace std;

constexpr ::std::size_t MaxReceivePerLoop            = 4;
constexpr ::std::size_t MaxSendPerLoop               = 4;
constexpr ::std::size_t MaxDataPerRead               = 1024 * 1024;
constexpr ::std::size_t MaxPendingIncomingConnection = 10;

constexpr ::std::size_t SendBufferSize    = 1024 * 1024;
constexpr ::std::size_t ReceiveBufferSize = 1024 * 1024;
constexpr bool   KeepAlive         = true;

struct MonitoringRequestEvent : public Actor::Event
{
    MonitoringRequestEvent(const string &topic) : m_topic(topic) {}
    string m_topic;
};

template <class _TObject> struct MonitoringDataEvent : public Actor::Event
{
    MonitoringDataEvent(const _TObject &obj) : m_obj(obj) {}
    _TObject m_obj;
};

struct MonitoringUknowDataEvent : public Actor::Event
{

    MonitoringUknowDataEvent(const string &obj) : m_topic(obj) {}
    string m_topic;
};

struct Latency
{
    int m_counter = 0;
};

struct Throughput
{
    int m_counter = 0;
};

class MyMonitoringService : public Actor
{
    public:
    MyMonitoringService() : m_pipe(*this)
    {
        registerEventHandler<MonitoringRequestEvent>(*this);
        registerUndeliveredEventHandler<MonitoringDataEvent<Latency>>(*this);
        registerUndeliveredEventHandler<MonitoringDataEvent<Throughput>>(*this);
    }

    void onEvent(const MonitoringRequestEvent &e)
    {
        m_pipe.setDestinationActorId(e.getSourceActorId());
        if (e.m_topic.compare("latency") == 0)
        {
            auto ev = m_pipe.push<MonitoringDataEvent<Latency>>(m_latency);
            m_latency.m_counter++;
        }
        else if (e.m_topic.compare("throughput") == 0)
        {
            auto ev = m_pipe.push<MonitoringDataEvent<Throughput>>(m_throughput);
            m_throughput.m_counter++;
        }
    }

    void onUndeliveredEvent(const MonitoringDataEvent<Latency> &event)
    {
        (void)event;
        cout << "undelivered event: MonitoringDataEvent<Latency>" << endl;
    }

    void onUndeliveredEvent(const MonitoringDataEvent<Throughput> &event)
    {
        (void)event;
        cout << "undelivered event: MonitoringDataEvent<Throughput>" << endl;
    }

    private:
    Latency    m_latency;
    Throughput m_throughput;

    Event::Pipe m_pipe;
};

struct MonitoringTag : public Service
{
};

using MyHttpNetwork = Network<MaxReceivePerLoop, MaxSendPerLoop, MaxDataPerRead, MaxPendingIncomingConnection>;

class MyHttpServerProcess : public HttpServerProcess<MyHttpNetwork, SendBufferSize, ReceiveBufferSize, KeepAlive>
{
    public:
    using parent = HttpServerProcess<MyHttpNetwork, SendBufferSize, ReceiveBufferSize, KeepAlive>;

    MyHttpServerProcess()
        : parent(parent::Parameters("")),
          m_monitoringPipe(*this, getEngine().getServiceIndex().getServiceActorId<MonitoringTag>())
    {
        assert(!"should never been call");
        throw runtime_error("should never been call: exists only for compilation need");
    }

    MyHttpServerProcess(const parent::Parameters &param)
        : parent(param), m_monitoringPipe(*this, getEngine().getServiceIndex().getServiceActorId<MonitoringTag>())
    {
        registerEventHandler<MonitoringDataEvent<Latency>>(*this);
        registerEventHandler<MonitoringDataEvent<Throughput>>(*this);
        registerEventHandler<MonitoringUknowDataEvent>(*this);
    }

    void onEvent(const MonitoringDataEvent<Latency> &e) { sendJson(serialize(e.m_obj)); }
    void onEvent(const MonitoringDataEvent<Throughput> &e) { sendJson(serialize(e.m_obj)); }
    void onEvent(const MonitoringUknowDataEvent &e)
    {
        send("HTTP/1.1 404 Not Found \r\n\r\n");
        send("No monitoring data for [");
        send(e.m_topic);
        send("] has been found");
        parent::registerCallback(m_selfDestructAfterResponseCallback);
    }

    protected:
    string serialize(const Latency &latency)
    {
        ostringstream os;
        os << "{"
           << "\"latency\":" << latency.m_counter << "}";
        return os.str();
    }

    string serialize(const Throughput &throughput)
    {
        ostringstream os;
        os << "{"
           << "\"throughput\":" << throughput.m_counter << "}";
        return os.str();
    }

    void sendJson(const string &serializedData)
    {
        sendHeader(200, "application/json", serializedData.size());
        send(serializedData);
        if (!KeepAlive)
            registerCallback(m_selfDestructAfterResponseCallback);
    }

    void onGetRequest(const string &header, const string &body) override
    {
        (void)header;
        (void)body;
        const string         path       = getPath(header);
        const vector<string> parsedPath = getParsedPath(path);

        if (parsedPath.size() == 0)
        {
            sendDocument("index.html");
            if (!KeepAlive)
                registerCallback(m_selfDestructAfterResponseCallback);
        }
        else if (parsedPath.size() > 1 && parsedPath[0].compare("monitoring") == 0)
        {
            auto ev    = m_monitoringPipe.push<MonitoringRequestEvent>(parsedPath[1]);
            ev.m_topic = parsedPath[1];
        }
        else if (parsedPath.size() > 1 && parsedPath[0].compare("autorefresh") == 0)
        {
            const string resp = string("<script type=\"text/javascript\">setInterval(function() {\r\n"
                                       "var xmlhttp = new XMLHttpRequest();\r\n"
                                       "xmlhttp.onreadystatechange = function() {\r\n"
                                       "if (this.readyState == 4 && this.status == 200){\r\n"
                                       "console.log(this.responseText);\r\n"
                                       "console.log(JSON.parse(this.responseText));\r\n"
                                       "}}\r\n"
                                       "xmlhttp.open('GET', '/monitoring/" +
                                       parsedPath[1] +
                                       "',true);\r\n"
                                       "xmlhttp.send();\r\n"
                                       "}, 1000);\r\n"
                                       "</script>");
            sendHeader(200, "text/html; charset=utf-8", resp.size());
            send(resp);
            if (!KeepAlive)
                registerCallback(m_selfDestructAfterResponseCallback);
        }
        else
        {
            sendDocument(path);
            if (!KeepAlive)
                registerCallback(m_selfDestructAfterResponseCallback);
        }
    }

    Event::Pipe m_monitoringPipe;
};

class MyHttpServer : public HttpServer<MyHttpNetwork, MyHttpServerProcess>
{
    public:
    using parent = HttpServer<MyHttpNetwork, MyHttpServerProcess>;

    MyHttpServer(const parent::Parameters &param) : parent(param) { listen(); }

    protected:
    void listen() override
    {
        cout << getActorId() << " registering listen" << endl;
        parent::listen();
    }

    void onListenSucceed() noexcept override { cout << getActorId() << " listen succeed" << endl; }
};

int main()
{
    Engine::StartSequence startSequence;
    startSequence.addServiceActor<MonitoringTag, MyMonitoringService>(3);
    //const string rootPath = get_current_dir_name(); // TODO: take pwd from config or deallocate
    startSequence.addActor<MyHttpServer>(2, MyHttpServer::Parameters(/*"127.0.0.1"*/ ""/**/, 8080/*, rootPath + string("/www")*/));
    Engine engine(startSequence);
    std::cout << "Press Enter to exit..." << std::endl;
    getchar();
    return 0;
}