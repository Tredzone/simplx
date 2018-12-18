
#include <cassert>
#include <mutex>
#include <condition_variable>

#include "trz/util/waitcondition.h"

using namespace std;

//---- Wait Condition IMPLEMENTATION -------------------------------------------

class WaitCondition: public IWaitCondition
{
public:
    WaitCondition()
        : m_Mutex(), m_DoneFlag(false)
    {
    }
    
    ~WaitCondition()
    {
        unique_lock<mutex> lock(m_Mutex);
        
        assert(m_DoneFlag);
    }
    void wait(void) override
    {
        unique_lock<mutex> lock(m_Mutex);
        
       m_Condition.wait(lock, [&]{return m_DoneFlag;});
    }
    void notify(void) override
    {
        {	lock_guard<mutex>	lock(m_Mutex);
			if (m_DoneFlag)				return;							// silent return
		
			m_DoneFlag = true;
		}
        
		m_Condition.notify_all();
    }

private:

    mutex				m_Mutex;
	condition_variable	m_Condition;
    bool                m_DoneFlag;
};


//---- INSTANTIATE -------------------------------------------------------------

// static
IWaitCondition*     IWaitCondition::Create(void)
{
        return new WaitCondition();
}
