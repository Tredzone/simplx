/*
 * timer.cpp
 *
 * This tutorial shows how to use a timer by starting a timer service and deriving a TimerProxy.
 * It'll wait 3 times for 1 second and then destroy itself
 */

#include <iostream>
#include <iomanip>
#include <string>

#include "simplx.h"

#include "trz/pattern/timer.h"

using namespace std;
using namespace tredzone;

class MyActor: public Actor, public timer::TimerProxy
{
public:
	MyActor()
        : TimerProxy(static_cast<tredzone::Actor&>(*this)), m_Count(0)
	{
		setRepeat(Time::Second(1));
	}
	
	void onTimeout(const tredzone::DateTime&) throw() override
	{
		m_Count++;
		cout << "Timer count " << m_Count << endl;
        
        if (m_Count == 3)   requestDestroy();
	}
	
private:

	int m_Count;
};

//---- MAIN --------------------------------------------------------------------

int main()
{
	cout << "tutorial #10 : timer" << endl;
    
    Engine::StartSequence	startSequence;
	
    // add timer service on CPU core #0
	startSequence.addServiceActor<service::Timer, timer::TimerActor>(0);
    // add MyActor on CPU core #0
	startSequence.addActor<MyActor>(0);

	Engine	engine(startSequence);
	
    cout << "Press enter to exit..." << endl;
    cin.get();
	
	return 0;
}
