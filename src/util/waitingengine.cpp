// waiting engine

#include <mutex>
#include <condition_variable>

#include "simplx.h"

#include "trz/util/waitingengine.h"

using namespace std;
using namespace tredzone;

//---- Waiting Engine ----------------------------------------------------------

class WaitingEngine: public virtual IWaitingEngine, public Engine
{
public:
	// ctor
	WaitingEngine(StartSequence& start_seq)
		: Engine(start_seq), m_Mutex(), m_DoneFlag(false)
	{
    }
    
    ~WaitingEngine()
    {
    }
    
    void    waitExplicitExit(void) override
    {
        unique_lock<mutex> lock(m_Mutex);
			
        m_Condition.wait(lock, [&]{return m_DoneFlag;});
    }
    
    void	allowExit(void) override
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
    bool				m_DoneFlag;
};

//---- INSTANTIATE -------------------------------------------------------------

// static
IWaitingEngine*     IWaitingEngine::Create(Engine::StartSequence& start_seq)
{
    return new WaitingEngine(start_seq);
}
    