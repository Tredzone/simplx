/**
 * @file keyboardactor.h
 * @brief asynchronous keyboard actor service utility
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

#include <memory>

#include "trz/engine/actor.h"

namespace tredzone
{

// subscribe event
struct KeyboardSubscribeEvent: public Actor::Event
{
};

// unsubscribe event
struct KeyboardUnsubscribeEvent: public Actor::Event
{
};

// keyboard key event    
struct KeyboardEvent: public Actor::Event
{
    // ctor
    KeyboardEvent(const char &c)
        : m_C(c)
    {
    }
    
    const char  m_C;
};

class IKeyboard
{
public:
    virtual ~IKeyboard() = default;
    
    virtual char    getAnyDownKey(void) = 0;
    
    static
    IKeyboard*  Create(void);
};

class KeyboardActor: public Actor, public Actor::Callback
{
public:    
    KeyboardActor();

    void    onCallback(void);
    void    onEvent(const KeyboardSubscribeEvent &e);
    void    onEvent(const KeyboardUnsubscribeEvent &e);
    
    struct ServiceTag: public Service{};
    
private:
    
    std::unique_ptr<IKeyboard>  m_Keyboard;
    ActorId                     m_Subscriber;
};


} // namespace tredzone


