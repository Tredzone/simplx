/*
 * main.cpp
 *
 * This tutorial will demonstrate the difference between a ReferecedActor and an UnreferencedActor.
 */

#include <iostream>

#include "simplx.h"

using namespace std;
using namespace tredzone;


// Event sent from WriterActor to PrinterActor.
// payload is a string that'll be printed to console

struct PrintEvent: Actor::Event
{
	PrintEvent(const string& message)
        : message(message)
    {
	}
	const string message;
};

// when PrinterActor receives a PrintEvent, it display it to the console
class PrinterActor: public Actor
{
public:
    // ctor
	PrinterActor()
    {
		cout << "PrinterActor::PrinterActor()" << endl;
		registerEventHandler<PrintEvent>(*this);	    // we expect to receive a PrintEvent
	}

	// called when PrintEvent is received
	void onEvent(const PrintEvent& event)
    {
		printMessage(event.message, event.getSourceActorId());
	}

	void printMessage(const string& message, const ActorId& from)
    {
		cout << "PrinterActor::onEvent(): " << message << ", from " << from << endl;
	}
};

// WriterActor will send a PrintEvent with a given string message to the PrinterActor
class WriterActor: public Actor
{
public:
    // ctor
	WriterActor(const ActorId& printerActorId)
    {
		cout << "WriterActor::CTOR()" << endl;
		Event::Pipe pipe(*this, printerActorId);	// create unidirectional communication channel between WriterActor (this) and PrinterActor (printerActorId)
		pipe.push<PrintEvent>("Hello, World!");		// send PrintEvent using said pipe
	}
};

// StarterActor instantiates PrinterActor and WriterActors
class StarterActor: public Actor
{
public:
    // ctor
	StarterActor()
    {
		cout << "StarterActor::CTOR()" << endl;

		// PrinterActor is instantiated as a referenced actor so we can use direct method calls
		ActorReference<PrinterActor> printerActorRef = newReferencedActor<PrinterActor>();
		// as we have an ActorReference to PrinterActor, we can directly call printMessage
		printerActorRef->printMessage("Hello using printerActorRef", getActorId());

        // create WriterActors, passing them PrinterActor's id
		newUnreferencedActor<WriterActor>(printerActorRef->getActorId());
		newUnreferencedActor<WriterActor>(printerActorRef->getActorId());
	}
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    Engine::StartSequence startSequence;
    
    startSequence.addActor<StarterActor>(0/*CPU core*/);

    Engine engine(startSequence);

    cout << "Press enter to exit...";
    cin.get();

    return 0;
}
