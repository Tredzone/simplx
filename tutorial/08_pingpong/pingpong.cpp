/*
 * main.cpp
 *
 * This tutorial shows how Actors are instantiated and how they communicate with each other.
 * A PongActor is instantiated as a service on CPU core #2, then a PingActor is instantiated on CPU core #1.
 * A TravelLogEvent is sent from Ping to Pong and back to Ping, recording its location at each stop.
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
    }
};

//---- MAIN --------------------------------------------------------------------

int main()
{
	cout << "tutorial #8 : ping pong" << endl;
    
    Engine::StartSequence	startSequence;
	
    // add PongActor, as service, on CPU core #2
	startSequence.addServiceActor<PongTag, PongActor>(2);
    // add PingActor on CPU core #1
	startSequence.addActor<PingActor>(1);

	Engine	engine(startSequence);
	
    // wait for 2 seconds (see tutorial #9 "sync_exit" for more)
	sleep(2);
	
	return 0;
}
