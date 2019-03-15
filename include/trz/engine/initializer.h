/**
 * @file initializer.h
 * @brief engine start sequence
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include "trz/engine/engine.h"

#include <iostream>
#ifndef NDEBUG
#include <csignal>
#endif

namespace tredzone
{

struct InitializerExceptionHandler
{
    virtual ~InitializerExceptionHandler() noexcept {}
    virtual void onException(const std::exception *) noexcept = 0;
};

template <class _InitData> class InitializerStartSequence : public Engine::StartSequence
{
  public:
    class Callback;
    struct Actor;
    template <class _Init>
    InitializerStartSequence(const _Init &, const Engine::CoreSet & = Engine::FullCoreSet(),
                                  InitializerExceptionHandler * = 0,
                                  int = (0));

#ifndef NDEBUG
    static sig_atomic_t debugGetCoreInitializerSingletonCount() { return debugCoreInitializerSingletonCount(0); }
#endif

  private:
    struct CoreInitializer;
    struct CoreInitializerSingleton;
    struct InitData
    {
        _InitData data;
        ::tredzone::Actor::ActorId coreInitializerSingletonActorId[ ::tredzone::Actor::MAX_NODE_COUNT];
        size_t coreInitializerSingletonCount;
        template <class _Init> inline InitData(const _Init &pinit) : data(pinit), coreInitializerSingletonCount(0) {}
        inline void addCoreInitializerSingleton(const ::tredzone::Actor::ActorId &actorId) noexcept
        {
            removeCoreInitializerSingleton(actorId);
            assert((int)coreInitializerSingletonCount < ::tredzone::Actor::MAX_NODE_COUNT);
            coreInitializerSingletonActorId[coreInitializerSingletonCount] = actorId;
            ++coreInitializerSingletonCount;
        }
        inline void removeCoreInitializerSingleton(const ::tredzone::Actor::ActorId &actorId) noexcept
        {
            size_t i = 0;
            for (; i < coreInitializerSingletonCount && coreInitializerSingletonActorId[i] != actorId; ++i)
            {
            }
            if (i < coreInitializerSingletonCount)
            {
                for (++i; i < coreInitializerSingletonCount; ++i)
                {
                    coreInitializerSingletonActorId[i - 1] = coreInitializerSingletonActorId[i];
                }
                --coreInitializerSingletonCount;
            }
        }
        inline const ::tredzone::Actor::ActorId *
        getNextCoreInitializerSingleton(const ::tredzone::Actor::ActorId &actorId) const noexcept
        {
            size_t i = 0;
            for (; i < coreInitializerSingletonCount && coreInitializerSingletonActorId[i] != actorId; ++i)
            {
            }
            if (i < coreInitializerSingletonCount && coreInitializerSingletonCount > 1)
            {
                return &coreInitializerSingletonActorId[(i + 1) % coreInitializerSingletonCount];
            }
            else
            {
                return 0;
            }
        }
    };
    typedef CacheLineAlignedObject<InitData> CacheAlignedInitData;
    std::unique_ptr<CacheAlignedInitData> initDataAutoPtr;
    CacheAlignedInitData *initData;
    InitializerExceptionHandler *exceptionHandler;

#ifndef NDEBUG
    static sig_atomic_t debugCoreInitializerSingletonCount(int inc)
    {
        static sig_atomic_t count = 0;

        if (inc > 0)
        {
            return atomicAddAndFetch(&count, (unsigned)inc);
        }
        else if (inc < 0)
        {
            return atomicSubAndFetch(&count, (unsigned)-inc);
        }
        else
        {
            return count;
        }
    }
#endif
};

template <class _InitData>
class InitializerStartSequence<_InitData>::Callback : private MultiDoubleChainLink<Callback>
{
  public:
    inline Callback(::tredzone::Actor &);
    virtual ~Callback() noexcept
    {
        if (withinCallbackChain != 0)
        {
            withinCallbackChain->remove(this);
        }
    }
    inline bool isInitialized() const noexcept { return withinCallbackChain == 0; }

  protected:
    virtual void onInitialize(_InitData &)
    {
        withinCallbackChain->remove(this);
        withinCallbackChain = 0;
        coreInitializerSingleton.reset();
    }

  private:
    friend class InitializerStartSequence;
    struct Chain : Callback::template DoubleChain<0u, Chain>
    {
        inline static Callback *getItem(MultiDoubleChainLink<Callback> *link) noexcept
        {
            return static_cast<Callback *>(link);
        }
        inline static const Callback *getItem(const MultiDoubleChainLink<Callback> *link) noexcept
        {
            return static_cast<const Callback *>(link);
        }
        inline static MultiDoubleChainLink<Callback> *getLink(Callback *item) noexcept
        {
            return static_cast<MultiDoubleChainLink<Callback> *>(item);
        }
        inline static const MultiDoubleChainLink<Callback> *getLink(const Callback *item) noexcept
        {
            return static_cast<const MultiDoubleChainLink<Callback> *>(item);
        }
    };

    ::tredzone::Actor::ActorReference<CoreInitializerSingleton> coreInitializerSingleton;
    Chain *withinCallbackChain;
};

template <class _InitData>
struct InitializerStartSequence<_InitData>::Actor : ::tredzone::Actor,
                                                         InitializerStartSequence<_InitData>::Callback
{
    inline Actor() : InitializerStartSequence<_InitData>::Callback(static_cast<::tredzone::Actor &>(*this)) {}
};

template <class _InitData>
struct InitializerStartSequence<_InitData>::CoreInitializerSingleton : ::tredzone::Actor,
                                                                            ::tredzone::Actor::Callback
{
    struct HandOverEvent : ::tredzone::Actor::Event
    {
    };

    CacheAlignedInitData *initData;
    InitializerExceptionHandler *exceptionHandler;
    typename InitializerStartSequence::Callback::Chain initializeInProgressCallbackChain;
    bool destroyFlag;

    CoreInitializerSingleton(InitializerStartSequence *startSequence = 0) : destroyFlag(true)
    {
        if (startSequence != 0)
        {
            initData = startSequence->initData;
            exceptionHandler = startSequence->exceptionHandler;
            registerEventHandler<HandOverEvent>(*this);
            startSequence->initDataAutoPtr.release();
            (*initData)->addCoreInitializerSingleton(*this);
            if ((*initData)->coreInitializerSingletonCount == 1)
            {
                this->::tredzone::Actor::registerCallback(*this);
            }
            destroyFlag = false;
        }
#ifndef NDEBUG
        InitializerStartSequence::debugCoreInitializerSingletonCount(1);
#endif
    }
#ifndef NDEBUG
    virtual ~CoreInitializerSingleton() noexcept
    {
        InitializerStartSequence::debugCoreInitializerSingletonCount(-1);
    }
#endif
    void onEvent(const HandOverEvent &) { onCallback(); }
    void onCallback() noexcept
    {
        typename InitializerStartSequence::Callback::Chain local;
        while (!initializeInProgressCallbackChain.empty())
        {
            typename InitializerStartSequence::Callback *callback = initializeInProgressCallbackChain.pop_front();
            (callback->withinCallbackChain = &local)->push_back(callback);
            try
            {
                callback->onInitialize((*initData)->data);
            }
            catch (const std::exception &e)
            {
                if (exceptionHandler != 0)
                {
                    exceptionHandler->onException(&e);
                }
                else
                {
                    onCallbackExceptionDefault(&e);
                }
            }
            catch (...)
            {
                if (exceptionHandler != 0)
                {
                    exceptionHandler->onException(0);
                }
                else
                {
                    onCallbackExceptionDefault(0);
                }
            }
        }
        local.push_back(initializeInProgressCallbackChain);
        while (!local.empty())
        {
            typename InitializerStartSequence::Callback *callback = local.pop_front();
            (callback->withinCallbackChain = &initializeInProgressCallbackChain)->push_back(callback);
        }
        const ::tredzone::Actor::ActorId *nextCoreInitializerSingleton =
            (*initData)->getNextCoreInitializerSingleton(*this);
        if (nextCoreInitializerSingleton != 0)
        {
            try
            {
                Event::Pipe(*this, *nextCoreInitializerSingleton).push<HandOverEvent>();
            }
            catch (const std::bad_alloc &)
            {
                this->::tredzone::Actor::registerCallback(*this);
                return;
            }
        }
        if (initializeInProgressCallbackChain.empty())
        {
            destroyFlag = true;
            (*initData)->removeCoreInitializerSingleton(*this);
            if (nextCoreInitializerSingleton == 0)
            {
                delete initData;
            }
        }
        else if (nextCoreInitializerSingleton == 0)
        {
            this->::tredzone::Actor::registerCallback(*this);
        }
    }
    virtual void onDestroyRequest() noexcept
    {
        if (destroyFlag)
        {
            ::tredzone::Actor::onDestroyRequest();
        }
        else
        {
            requestDestroy();
        }
    }
    static void onCallbackExceptionDefault(const std::exception *e) noexcept
    {
        std::cout << "tredzone::InitializerStartSequence::Callback::onInitialize() threw an exception ["
                  << (e == 0 ? "?" : e->what()) << ']' << std::endl;
        exit(-1);
    }
};

template <class _InitData> struct InitializerStartSequence<_InitData>::CoreInitializer : ::tredzone::Actor
{
    ActorReference<CoreInitializerSingleton> coreInitializerSingleton;

    CoreInitializer(InitializerStartSequence *startSequence)
        : coreInitializerSingleton(newReferencedSingletonActor<CoreInitializerSingleton>(startSequence))
    {
        requestDestroy();
    }
};

template <class _InitData>
InitializerStartSequence<_InitData>::Callback::Callback(::tredzone::Actor &actor)
    : coreInitializerSingleton(actor.::tredzone::Actor::newReferencedSingletonActor<CoreInitializerSingleton>()),
      withinCallbackChain(&coreInitializerSingleton->initializeInProgressCallbackChain)
{
    withinCallbackChain->push_back(this);
}

template <class _InitData>
template <class _Init>
InitializerStartSequence<_InitData>::InitializerStartSequence(
    const _Init &pinit, const Engine::CoreSet &pcoreSet, InitializerExceptionHandler *pexceptionHandler, int)
    : Engine::StartSequence(pcoreSet, 0), initDataAutoPtr(new CacheAlignedInitData(pinit)),
      initData(initDataAutoPtr.get()), exceptionHandler(pexceptionHandler)
{
    for (size_t i = 0, endi = pcoreSet.size(); i != endi; ++i)
    {
        addActor<CoreInitializer>(pcoreSet.at((::tredzone::Actor::NodeId)i), this);
    }
}

} // namespace