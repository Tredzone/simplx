
#pragma once

#include "trz/engine/engine.h"

class IWaitingEngine: public tredzone::IEngine
{
public:
    virtual ~IWaitingEngine() = default;
    
    virtual void    waitExplicitExit(void) = 0;
    virtual void	allowExit(void) = 0;
    
    static
    class IWaitingEngine*     Create(class tredzone::Engine::StartSequence& start_seq);
};