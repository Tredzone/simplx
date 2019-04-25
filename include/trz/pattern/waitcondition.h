
#pragma once

#include <chrono>

//---- Wait Condition PIMPL ----------------------------------------------------

class IWaitCondition
{
public:
    
    // dtor
    virtual ~IWaitCondition() = default;
    
    virtual void    wait(void) = 0;
    virtual bool    wait_for(const std::chrono::duration<int64_t> &delat_t) = 0;        // returns timed_out_f
    virtual void    notify(void) = 0;
    
    static
    IWaitCondition*     Create(void);
};

// nada mas
