/*
 * sync_exit.cpp
 *
 * This tutorial shows how Actors are instantiated and how they communicate with each other.
 * A PongActor is instantiated as a service on CPU core #2, then a PingActor is instantiated on CPU core #1.
 * A TravelLogEvent is sent from Ping to Pong and back to Ping, recording its location at each stop.
 *
 * This tutorial also shows how to terminate the workflow deterministically by overriding PingActor's
 * onDestroyRequest() method and either accepting the destruction request, if the workflow is done, or
 * postponing the destruction request, if work is still ongoing.
 */

#include <iostream>
#include <iomanip>
#include <string>

#include "simplx.h"

using namespace std;
using namespace tredzone;

//---- Travel Event between actors ---------------------------------------------

struct TravelLogEvent : Actor::Event
{
	TravelLogEvent(const string &place)
		: visited(place)
	{
	}
	
	string	visited;
};

//---- Pong Actor Service ------------------------------------------------------

class PongActor : public Actor
{
public:
    PongActor()
	{
    	cout << "PongActor::CTOR" << endl;
        registerEventHandler<TravelLogEvent>(*this);
    }

    // upon receiving a TravelLogEvent (from Ping)
    void onEvent(const TravelLogEvent& e)
	{
        // append our location & send it back to sender (Ping)
        Event::Pipe	pipe(*this, e.getSourceActorId());
		
        pipe.push<TravelLogEvent>(e.visited + ", visited Pong @ core #" + to_string((int)getCore()));
    }
};

// tag identifying Pong's service
struct PongTag : public Service{};

//---- Ping Actor --------------------------------------------------------------

class PingActor : public Actor
{
public:
    PingActor()
        : m_Done(false)
	{
    	cout << "PingActor::CTOR" << endl;
        registerEventHandler<TravelLogEvent>(*this);
		
		// send a TravelLogEvent to Pong
		const ActorId	destinationActorId = getEngine().getServiceIndex().getServiceActorId<PongTag>();
        
        Event::Pipe pipe(*this, destinationActorId);
		
        pipe.push<TravelLogEvent>("started in Ping");
    }

    // upon receiving a TravelLogEvent 
    void	onEvent(const TravelLogEvent& e)
	{
        // output all places visited
		cout << "travel log: " << e.visited << ", ended back in Ping @ core #" << to_string((int)getCore()) << endl;
        
        // flag work as done
        m_Done = true;
        
        // request this actor's destruction
        requestDestroy();
    }
    
    void    onDestroyRequest(void) noexcept override
    {
        if (m_Done)
                acceptDestroy();        // accept to destroy this actor
        else    requestDestroy();       // retry at next event loop iteration
    }
    
private:

    bool    m_Done;
};

//---- MAIN --------------------------------------------------------------------

int main()
{
	cout << "tutorial #9 : sync exit" << endl;
    
    Engine::StartSequence	startSequence;
	
    // add PongActor, as service, on CPU core #2
	startSequence.addServiceActor<PongTag, PongActor>(2);
    // add PingActor on CPU core #1
	startSequence.addActor<PingActor>(1);

	Engine	engine(startSequence);
	
    return 0;
}
