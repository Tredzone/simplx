/*
 * multi_callback.cpp
 *
 * This tutorial illustrates how an Actor can handle multiple callbacks
 * There are two ways to register a callback:
 * - an actor can derive from Actor::Callback
 * - declare a callback handler member
 */

#include <iostream>
#include <simplx.h>

using namespace std;
using namespace tredzone;

// MyActor derives both from Actor and Actor::Callback in which case it's also a callback handler
class MyActor : public Actor, public Actor::Callback
{
public:

	// ctor
    MyActor()
        : m_MyCallbackHandler()
    {
		registerCallback(*this);	            // as MyActor is a callback handler, it can register itself
		registerCallback(m_MyCallbackHandler);	// register a callback using a callback handler member (other than itself)
	}

	// as MyActor is a callback handler, it must implement the onCallback method
	void onCallback()
    {
		cout << "MyActor::onCallback()" << endl;
	}

private:
	
    // a (custom) callback handler can be defined by deriving from Actor::Callback
	class MyCallbackHandler : public Actor::Callback
    {
    public:
		void onCallback()
        {
			cout << "MyCallbackHandler::onCallback()" << endl;
		}
	};

	MyCallbackHandler   m_MyCallbackHandler;	// used as member variable
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    cout << "tutorial #5 : multi callback" << endl;
    
    Engine::StartSequence   startSequence;
    startSequence.addActor<MyActor>(0);

    Engine engine(startSequence);

    cout << "Press enter to exit..."<< endl;
    cin.get();

    return 0;
}
