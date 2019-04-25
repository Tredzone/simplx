
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <chrono>

#include "trz/pattern/waitcondition.h"

using namespace std;
using namespace std::chrono;

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
        // unique_lock<mutex> lock(m_Mutex);
        
        // assert(m_DoneFlag);
    }
    void wait(void) override
    {
        unique_lock<mutex> lock(m_Mutex);
        
        m_Condition.wait(lock, [&]{return m_DoneFlag;});
    }
    
    bool    wait_for(const std::chrono::duration<int64_t> &delay_t) override
    {
        unique_lock<mutex> lock(m_Mutex);
        
        const bool  timed_out_f = (cv_status::timeout == m_Condition.wait_for(lock, delay_t));
        return timed_out_f;
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
