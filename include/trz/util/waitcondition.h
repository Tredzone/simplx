
#pragma once

//---- Wait Condition PIMPL ----------------------------------------------------

class IWaitCondition
{
public:
    
    // dtor
    virtual ~IWaitCondition() = default;
    
    virtual void    wait(void) = 0;
    virtual void    notify(void) = 0;
    
    static
    IWaitCondition*     Create(void);
};

// nada mas
