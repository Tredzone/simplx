/*
 * printer_actor_starter.cpp
 *
 * This tutorial demonstrates how Actors are started and how they communicate with each other.
 * A PrinterActor will be instantiated and will be waiting to receive PrintEvents.
 * Several WriterActors will be started and will send a PrintEvent to the PrinterActor.
 */

#include <iostream>

#include "simplx.h"

using namespace tredzone;
using namespace std;

/*
 * Event that will be sent from the WriterActor to the PrinterActor.
 * Payload is a string that will be printed to the console.
 */
struct PrintEvent : Actor::Event
{
	PrintEvent(const string& message) : message(message)
    {
	}
    
	const string message;
};

// PrinterActor will receive a PrintEvent and display it to the console
class PrinterActor : public Actor
{
public:

    // CTOR
	PrinterActor()
    {
		cout << "PrinterActor::PrinterActor()" << endl;
		registerEventHandler<PrintEvent>(*this);	    // Actor expects to receive a PrintEvent
	}

	// called when PrintEvent is received
	void onEvent(const PrintEvent& event)
    {
		cout << "PrinterActor::onEvent(): " << event.message << ", from ActorId " << event.getSourceActorId() << endl;
	}
};

// WriterActor will send a PrintEvent containing a string message to PrinterActor
class WriterActor : public Actor
{
public:
	WriterActor(const ActorId& printerActorId)
    {
		cout << "WriterActor::CTOR" << endl;
        
        Event::Pipe pipe(*this, printerActorId);	// create a uni-directional communication channel between WriterActor (*this) and PrinterActor (printerActorId)
		pipe.push<PrintEvent>("Hello, World!");		// send PrintEvent using said pipe
	}
};

// StarterActor will start the PrinterActor and the WriterActors
class StarterActor : public Actor
{
public:
	StarterActor()
    {
		cout << "StarterActor::CTOR" << endl;
		ActorId printActorAddress = newUnreferencedActor<PrinterActor>();	// create the PrinterActor and store its address (ActorId)
		newUnreferencedActor<WriterActor>(printActorAddress);				// create a WriterActor giving it the PrinterActor's address
		newUnreferencedActor<WriterActor>(printActorAddress);
	}
};

//---- Main --------------------------------------------------------------------

int main()
{
    cout << "tutorial #3 : printer actor starter" << endl;
    
    Engine::StartSequence   startSequence;      // configure the initial Actor system
    
    // the addActor methods take as first argument the CPU CoreId on which given Actor will be running
    startSequence.addActor<StarterActor>(0);

    Engine   engine(startSequence);
    
    sleep(2);                                   // (see tuto #9 for synchronous exit)
    
    return 0;
}
