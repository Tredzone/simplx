/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file serverprocess.hpp
 * @brief http server process
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "simplx.h"
#include "trz/connector/tcpconnector.hpp"
#include "trz/connector/http/server/server.hpp"
#include "trz/util/timer.h"

#include <fstream>
#include <sstream>
#include <vector>

namespace tredzone
{
namespace connector
{
namespace http
{

using ::std::size_t;
using ::std::ifstream;
using ::std::ostringstream;
using ::std::string;
using ::std::vector;

/**
 * @brief communication class handling new http connection
 * 
 * @tparam _Network network actor managing connection and communication to network (low level)
 * @tparam _SendBufferSize 
 * @tparam _ReceiveBufferSize 
 * @tparam _KeepAlive if false, server process will self-terminate after response
 * @tparam _MaxHeaderSize limit of header (in byte units)
 */
template <class _Network, size_t _SendBufferSize, size_t _ReceiveBufferSize, bool _KeepAlive, size_t _MaxHeaderSize = 4096>
class HttpServerProcess : public TcpClient<_Network, _SendBufferSize, _ReceiveBufferSize>
{
    using parent = TcpClient<_Network, _SendBufferSize, _ReceiveBufferSize>;

    public:
    class Parameters
    {
        public:
        Parameters(const string &rootPath) : m_rootPath(rootPath) {}

        const string m_rootPath;
    };

    /**
     * @brief Instantiate new Http Server Process 
     * 
     * @param param 
     */
    HttpServerProcess(const Parameters &param)
        : m_selfDestructAfterResponseCallback(*this), m_rootPath(param.m_rootPath)
    {
    }

    /**
     * @brief Destroy Http Server Process 
     * 
     */
    ~HttpServerProcess() {}

    /**
     * @brief Get Path requested by client
     * 
     * @param httpHeader header of http request
     * @return string 
     */
    string getPath(const string &httpHeader)
    {
        size_t pathBegin = httpHeader.find(' ') + 1;
        if (pathBegin == string::npos)
            return "";
        size_t paramBegin = httpHeader.find('?', pathBegin);
        if (paramBegin != string::npos)
        {
            return httpHeader.substr(pathBegin, paramBegin - pathBegin);
        }
        size_t pathEnd = httpHeader.find(' ', pathBegin);
        if (pathEnd == string::npos)
            return "";
        return httpHeader.substr(pathBegin, pathEnd - pathBegin);
    }

    /**
     * @brief return list of split parent directories from path header
     * 
     * @param full path in header
     * @return vector<string> of partial path
     */
    vector<string> getParsedPath(string path)
    {
        vector<string> nodes;
        size_t         index = path.find('/', 1);
        while (index != string::npos)
        {
            if (index > 1)
                nodes.push_back(path.substr(1, index - 1));
            path.erase(0, index);
            index = path.find('/', 1);
        }
        if (path.length() > 1)
            nodes.push_back(path.substr(1));
        return nodes;
    }

    protected:    
    /**
     * @brief callback triggered when client receive new data.
     * as this is a HTTP client process, check end of http header
     * 
     * @param data received
     * @param dataSize data size
     */
    void onDataReceived(const uint8_t *data, size_t dataSize) noexcept override
    {
        if (m_state < 4)
        {   
            // state machine validating a complete http header
            for (uint64_t i = 0; i < dataSize; ++i)
            {
                if (m_state == 0 && data[i] == '\r')
                {
                    m_state++;
                }
                else if (m_state == 1 && data[i] == '\n')
                {
                    m_state++;
                }
                else if (m_state == 2 && data[i] == '\r')
                {
                    m_state++;
                }
                else if (m_state == 3 && data[i] == '\n')
                {
                    m_state++;
                    // header complete
                    m_httpHeader.write((const char *)data, i);
                    m_httpBody.write((const char *)data + i, dataSize - i);
                    const string header = m_httpHeader.str();

                    const size_t indexContentLength = header.find("Content-Length: ");
                    if (indexContentLength != string::npos)
                    {
                        const size_t begin = indexContentLength + 16;
                        const size_t end   = header.find("\r\n", begin);
                        m_bodyLengthToGet  = std::atoi(header.substr(begin, end - begin).c_str()) - (dataSize - i);
                        if (m_bodyLengthToGet > 0)
                            return;
                    }

                    const string body = m_httpBody.str();
                    onDataReceived(nullptr, 0);
                    return;
                }
                else
                    m_state = 0;
            }
            m_httpHeader.write((const char *)data, dataSize);

            // cancel parsing of abnormal http header request 
            if (m_httpHeader.tellp() > static_cast<ssize_t>(_MaxHeaderSize)) 
            {
                parent::send("HTTP/1.1 500 Internal Server Error\r\n\r\n");
                parent::send("Request header too large");
                parent::registerCallback(m_selfDestructAfterResponseCallback);
                clearBuffer();
                return;
            }
        }
        else
        {
            m_httpBody.write((const char *)data, std::min((int64_t)dataSize, (int64_t)m_bodyLengthToGet));
            m_bodyLengthToGet -= dataSize;
            if (m_bodyLengthToGet > 0)
            {
                return;
            }

            const string header = m_httpHeader.str();
            const string body   = m_httpBody.str();
            clearBuffer();

            onCompleteRequest(header, body);
        }
    }

    /**
     * @brief distpatch request by type
     * 
     * @param request http request header
     * @param body http request body
     */
    virtual void onCompleteRequest(const string &header, const string &body)
    {
        if (header.find("GET", 0, 3) != string::npos)
            onGetRequest(header, body);
        else if (header.find("POST", 0, 4) != string::npos)
            onPostRequest(header, body);
        else if (header.find("PUT", 0, 3) != string::npos)
            onPutRequest(header, body);
        else if (header.find("DELETE", 0, 6) != string::npos)
            onDeleteRequest(header, body);
        else if (header.find("PATCH", 0, 5) != string::npos)
            onPatchRequest(header, body);
    }

    /**
     * @brief callback triggered upon GET request
     * 
     * @param request http request header
     * @param body http request body
     */
    virtual void onGetRequest(const string &header, const string &body)
    {
        (void)header;
        (void)body;
    }

    /**
     * @brief callback triggered upon POST request
     * 
     * @param request http request header
     * @param body http request body
     */
    virtual void onPostRequest(const string &header, const string &body)
    {
        (void)header;
        (void)body;
    }

    /**
     * @brief callback triggered upon PUT request
     * 
     * @param request http request header
     * @param body http request body
     */
    virtual void onPutRequest(const string &header, const string &body)
    {
        (void)header;
        (void)body;
    }

    /**
     * @brief callback triggered upon DELETE request
     * 
     * @param request http request header
     * @param body http request body
    */
    virtual void onDeleteRequest(const string &header, const string &body)
    {
        (void)header;
        (void)body;
    }

    /**
     * @brief callback triggered upon PATCH request
     * 
     * @param request http request header
     * @param body http request body
    */
    virtual void onPatchRequest(const string &header, const string &body)
    {
        (void)header;
        (void)body;
    }

    /**
     * @brief send response header 
     * 
     * @param statusCode the status code 
     * @param contentType type of content in payload
     * @param length size of the payload
     */
    void sendHeader(int statusCode, const string &contentType, size_t length)
    {
        ostringstream outHeader;
        outHeader << "HTTP/1.1 " << statusCode << "\r\nContent-Type: " << contentType;
        if (_KeepAlive)
            outHeader << "\r\nConnection: keep-alive";
        else
            outHeader << "\r\nConnection: close";
        outHeader << "\r\nContent-Length: " << length << "\r\n\r\n";
        parent::send(outHeader.str());
    }

    /**
     * @brief send document present in root folder as response  
     * 
     * @param documentPath 
     */
    void sendDocument(const string &documentPath)
    {
        // get real full path from document path
        std::string realPath = m_rootPath + documentPath;
        char *tmp = canonicalize_file_name(realPath.c_str());
        if (!tmp)
        {
            // document not found
            parent::send("HTTP/1.1 404 Not Found \r\n\r\n");
            parent::send("Page not found");
            parent::registerCallback(m_selfDestructAfterResponseCallback);
            clearBuffer();
            return;
        }
        else
        {
            realPath = tmp;
            free(tmp);
        }

        // confirm root path is part of documment real path 
        if (realPath.compare(0, m_rootPath.length(), m_rootPath))
        {
            // if not (meaning path contains too much /..) reject the request for security
            parent::send("HTTP/1.1 403 Forbidden\r\n\r\n");
            parent::registerCallback(m_selfDestructAfterResponseCallback);
            clearBuffer();
            return;
        }

        // read file content
        ifstream f(realPath.c_str(), std::ios::in | std::ios::binary);
        if (!f.good())
        {
            parent::send("HTTP/1.1 404 Not Found \r\n\r\n");
            parent::send("Page not found");
            parent::registerCallback(m_selfDestructAfterResponseCallback);
            return;
        }

        // calculate file content size  
        f.seekg(0, std::ios::end);
        size_t len = f.tellg();

        if (len > _SendBufferSize)
        {
            // TODO: register callback to send missing portion on next loop
            parent::send("HTTP/1.1 500 Internal Server Error\r\n\r\n");
            parent::send("Content too large to be served (@admin: check serverSend buffer size) ");
            parent::registerCallback(m_selfDestructAfterResponseCallback);
            clearBuffer();
            return;
        }

        // put file content in buffer
        char *buff = new char[len];
        f.seekg(0, std::ios::beg);
        f.read(buff, len);
        f.close();

        // send buffer
        try
        {
            if (documentPath.find(".css") != std::string::npos) {
                sendHeader(200, "text/css", len);
            }
            else {
                sendHeader(200, "text/html", len); 
            }
            parent::send(buff, len);
        }
        catch (...)
        {
            // close connection on failure 
            parent::registerCallback(m_selfDestructAfterResponseCallback);
            clearBuffer();
        }
    }

    /**
     * @brief clear request buffer
     * 
     */
    void clearBuffer()
    {
        m_state = 0;
        m_httpHeader.str("");
        m_httpHeader.clear();
        m_httpBody.str("");
        m_httpBody.clear();
    }

    /**
     * @brief actorCallback to destroy client after response is sent 
     * 
     */
    class SelfDestructAfterResponseCallback : public Actor::Callback
    {
        public:
        HttpServerProcess &m_actor;
        SelfDestructAfterResponseCallback(HttpServerProcess &actor) : m_actor(actor) {}

        void onCallback(void)
        {
            if (m_actor.getSenderBufferContentSize() > 0)
            {
                m_actor.registerCallback(*this);
            }
            else
            {
                m_actor.destroy();
            }
        }
    };
    SelfDestructAfterResponseCallback m_selfDestructAfterResponseCallback;

    void onTimeout() noexcept
    {
        parent::registerCallback(m_selfDestructAfterResponseCallback);
        parent::m_sender.clearBuffer();
    }
    
    const string m_rootPath;

    ostringstream m_httpHeader;
    ostringstream m_httpBody;
    int64_t       m_state            = 0;
    bool          m_responseSentFlag = false;
    int           m_bodyLengthToGet  = -1;

    
};
} // namespace http
} // namespace connector
} // namespace tredzone