#pragma once

#include "channel.hpp"

#include <cctype>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace xthreadbus {
/**
 * @brief bus singleton containing channels
 * 
 * @tparam _TChannelId 
 * @tparam _MaxSize 
 */
template <class _TChannelId, std::size_t _MaxSize> class XThreadBus
{
    std::unordered_map<_TChannelId, Channel<_MaxSize> *> m_channels;
    std::unordered_set<_TChannelId>                      m_subscribers;
    std::unordered_set<_TChannelId>                      m_publishers;

    public:
    using Subscriber = typename Channel<_MaxSize>::Subscriber;
    using Publisher  = typename Channel<_MaxSize>::Publisher;

    /**
     * @brief Get Subscriber at first call of get
     * create channel if it doesn't exist
     * 
     * @param idChannel 
     * @return Subscriber& 
     */
    Subscriber &getSubscriber(_TChannelId idChannel)
    {
        std::lock_guard<std::mutex> lock(mutex());

        auto it = m_channels.find(idChannel);
        if (it == m_channels.end())
        {
            m_channels.emplace(idChannel, new Channel<_MaxSize>());
            it = m_channels.find(idChannel);
        }
        auto it2 = m_subscribers.find(idChannel);
        if (it2 != m_subscribers.end())
        {
            throw "channel's subscriber already in use";
        }
        m_subscribers.emplace(idChannel);
        return it->second->getSubscriber();
    }
    
    /**
     * @brief Get the Publisher at first call of get
     * create channel if it doesn't exist
     * 
     * @param idChannel 
     * @return Publisher& 
     */
    Publisher &getPublisher(_TChannelId idChannel)
    {
        std::lock_guard<std::mutex> lock(mutex());
        auto                        it = m_channels.find(idChannel);
        if (it == m_channels.end())
        {
            m_channels.emplace(idChannel, new Channel<_MaxSize>());
            it = m_channels.find(idChannel);
        }
        auto it2 = m_publishers.find(idChannel);
        if (it2 != m_publishers.end())
        {
            throw "channel's publisher already in use";
        }
        m_publishers.emplace(idChannel);
        return it->second->getPublisher();
    }

    /**
     * @brief get XThreadBus singleton
     * 
     * @return XThreadBus& 
     */
    static XThreadBus &get()
    {
        static XThreadBus instance;
        return instance;
    }

    /**
     * @brief get mutex singleton 
     * 
     * @return std::mutex& 
     */
    static std::mutex &mutex()
    {
        static std::mutex m_mutex;
        return m_mutex;
    }

    private:
    XThreadBus()                   = default;
    ~XThreadBus()                  = default;
    XThreadBus(const XThreadBus &) = delete;
    XThreadBus &operator=(const XThreadBus &) = delete;

};
} // namespace xthreadbus