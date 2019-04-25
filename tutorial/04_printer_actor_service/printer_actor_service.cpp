/*
 * printer_actor_service.cpp
 *
 * This tutorial demonstrates how to start Actors and how they communicate.
 * A ServiceActor is used to easily retrieve the PrinterActor from the service index.
 * The PrinterActor will be instantiated and will be waiting to receive PrintEvents.
 * Several WriterActors will be started and will send a PrintEvent to the PrinterActor.
 */

#include <iostream>

#include "simplx.h"

using namespace std;
using namespace tredzone;


/*
 * Event that will be sent from the WriterActor to the PrinterActor.
 * The payload is a string that will be printed to the console.
 */
struct PrintEvent : Actor::Event
{
	PrintEvent(const string& message)
        : message(message)
    {
	}
    
	const string message;
};

// when PrinterActor receives PrintEvents it displays them to the console
class PrinterActor : public Actor
{
public:

	// service tag (or key) used to uniquely identify service
	struct ServiceTag : Service {};

    // ctor
	PrinterActor()
    {
		cout << "PrinterActor::PrinterActor()" << endl;
		registerEventHandler<PrintEvent>(*this);
    }
    
	// called when PrintEvent is received
	void onEvent(const PrintEvent& event)
    {
		cout << "PrinterActor::onEvent(): " << event.message << ", from " << event.getSourceActorId() << endl;
	}
};

// WriterActor sends a PrintEvent containing a string to PrinterActor
class WriterActor : public Actor
{
public:
    // ctor
    WriterActor()
    {
		cout << "WriterActor::CTOR()" << endl;
        
        // retrieve PrinterActor's id from the ServiceIndex
		const ActorId&   printerActorId = getEngine().getServiceIndex().getServiceActorId<PrinterActor::ServiceTag>();
        
		Event::Pipe pipe(*this, printerActorId);	// create uni-directional communication channel between WriterActor (this) and PrinterActor (printerActorId)
		pipe.push<PrintEvent>("Hello, World!");		// send PrintEvent through pipe
	}
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    cout << "tutorial #4 : printer actor service" << endl;
    
    Engine::StartSequence   startSequence;	        // configure initial Actor system
    
    startSequence.addServiceActor<PrinterActor::ServiceTag, PrinterActor>(0/*CPU core*/);
    startSequence.addActor<WriterActor>(0/*CPU core*/);
    startSequence.addActor<WriterActor>(0/*CPU core*/);

    Engine engine(startSequence);	                // start above actors

    cout << "Press enter to exit...";
    cin.get();

    return 0;
}
