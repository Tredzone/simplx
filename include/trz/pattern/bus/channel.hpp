#pragma once

#include <cstdint>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>

#include "data.hpp"
#include "ringbuffer.hpp"


namespace xthreadbus {
/**
 * @brief holds chanel's specific publisher, consumer and transfer buffer
 * 
 * @tparam _MaxSize of exchange buffer
 */
template <std::size_t _MaxSize> class Channel
{
    public:
    /**
     * @brief communication sending part 
     * 
     */
    class Publisher
    {
        Channel &m_channel;

        public:
        /**
         * @brief Construct a new Publisher
         * 
         * @param channel to publish on 
         */
        Publisher(Channel &channel) : m_channel(channel), m_sendBuffer(_MaxSize) { m_sendBuffer.clear(); }
        
        /**
         * @brief store data pointer for later send
         * 
         * @tparam _TDataType data pointer type
         * @param data pointer to publish
         * @return _TDataType& ref to data
         */
        template <typename _TDataType> _TDataType &prepare(_TDataType *data)
        {
            static_assert(std::is_base_of<Data, _TDataType>::value, "_TDataType must extend Data");
            m_sendBuffer.push_back(data);
            return *data;
        }

        /**
         * @brief directly publish pointer data in transfer buffer
         * when transfer buffer is full, store data pointer to a later push
         * 
         * @tparam _TDataType data pointer type
         * @param data pointer to publish
         * @param storeOnFail when send fails, store for later send 
         * @return [sent] 
         */
        template <typename _TDataType> bool publishOne(_TDataType *data, bool storeOnFail = true)
        {
            static_assert(std::is_base_of<Data, _TDataType>::value, "_TDataType must extend Data");
            const bool sent = m_channel.m_ringbuffer.enqueue(data);
            if (!sent && storeOnFail)
            {
                // TODO? push front?
                m_sendBuffer.push_back(data);
            }
            return sent;
        }

        /**
         * @brief send all/maximum (until transfer buffer is full) 
         * prepared/stored data pointer all in a row.
         * 
         * @return size_t [data pointer left to be send]
         */
        std::size_t publishAll()
        {
            const std::size_t dataToSendSize = m_sendBuffer.size();
            if (!dataToSendSize)
            {
                return 0;
            }
            const std::size_t sent       = m_channel.m_ringbuffer.enqueue(m_sendBuffer.data(), dataToSendSize);
            const std::size_t leftToSend = dataToSendSize - sent;
            if (leftToSend)
            {
                memcpy(m_sendBuffer.data(), m_sendBuffer.data() + sent, leftToSend);
            }
            m_sendBuffer.erase(m_sendBuffer.begin() + leftToSend, m_sendBuffer.end());
            return leftToSend;
        }

        private:
        std::vector<Data *> m_sendBuffer;
    };

    /**
     * @brief communication receiving part
     * 
     */
    class Subscriber
    {
        /**
         * @brief helper casting data pointer type into subscribed type
         * 
         * @tparam _TDataType data pointer type
         * @tparam _TSubscriberType handler type having onData(const _TDataType &) method
         */
        template <class _TDataType, class _TSubscriberType> class StaticEventHandler
        {
            public:
            static void staticRead(void *subscriber, const Data &data)
            {
                reinterpret_cast<_TSubscriberType *>(subscriber)->onData(static_cast<const _TDataType &>(data));
            }
        };

        public:
        /**
         * @brief Construct a new Subscriber
         * 
         * @param channel 
         */
        Subscriber(Channel &channel) : m_channel(channel) {}

        /**
         * @brief subscribe handler to data pointer type
         * 
         * @tparam _TDataType data pointer type
         * @tparam _TSubscriberType handler type having onData(const _TDataType &) method
         * @param subscriber handler instance address
         */
        template <class _TDataType, class _TSubscriberType> void subscribe(_TSubscriberType *subscriber)
        {
            auto dataTypeId = std::type_index(typeid(_TDataType));
            if (m_subscribers.count(dataTypeId)) return ; // TODO ? : erase previously subscribed handler ?
            static_assert(std::is_base_of<Data, _TDataType>::value, "_TDataType must extend Data");
            void (*staticRead)(void *, const Data &) = &StaticEventHandler<_TDataType, _TSubscriberType>::staticRead;
            std::tuple<void *, void (*)(void *, const Data &)> consumeFunc = std::make_tuple(subscriber, staticRead);
            m_subscribers.emplace(dataTypeId, consumeFunc);
        }

        /**
         * @brief get next data pointer in transfer buffer
         * then callback handler onData method 
         * 
         * @return [data pointer found]
         */
        bool readOne()
        {
            const bool msgRead = m_channel.m_ringbuffer.dequeue(m_readBuffer.data(), 1)==1;
            std::size_t index = 0;
            if (msgRead && nullptr != m_readBuffer[index])
            {
                    const auto it = m_subscribers.find(std::type_index(typeid(*m_readBuffer[index])));
                    if (it != m_subscribers.end())
                    {
                        auto subscriber = std::get<0>(it->second);
                        auto onData     = std::get<1>(it->second);
                        onData(subscriber, *m_readBuffer[index]);
                    }
                    delete m_readBuffer[index];
                
            }
            return msgRead;
        }

        /**
         * @brief get all data pointer in transfer buffer
         * then callback all handlers onData method 
         * 
         * @return std::size_t amount of data pointer read
         */
        std::size_t readAll()
        {
            const std::size_t size = m_channel.m_ringbuffer.dequeue(m_readBuffer.data(), _MaxSize);
            for (std::size_t index = 0; index < size; ++index)
            {
                if (nullptr != m_readBuffer[index])
                {
                    const auto it = m_subscribers.find(std::type_index(typeid(*m_readBuffer[index])));
                    if (it != m_subscribers.end())
                    {
                        auto subscriber = std::get<0>(it->second);
                        auto onData     = std::get<1>(it->second);
                        onData(subscriber, *m_readBuffer[index]);
                    }
                    delete m_readBuffer[index];
                }
            }
            return size;
        }

        private:
        Channel &                                                                               m_channel;
        std::unordered_map<std::type_index, std::tuple<void *, void (*)(void *, const Data &)>> m_subscribers;
        std::array<Data *, _MaxSize>                                                            m_readBuffer;
    };

    public:
    /**
     * @brief Construct a new Channel
     * 
     */
    Channel() : m_publisher(*this), m_consumer(*this) {}

    Publisher & getPublisher() { return m_publisher; }
    Subscriber &getSubscriber() { return m_consumer; }

    private:
    Publisher                    m_publisher;
    Subscriber                   m_consumer;
    ringbuffer<Data *, _MaxSize> m_ringbuffer;
};

} // namespace xthreadbus