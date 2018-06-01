/*
 * main.cpp
 *
 * This tutorial will illustrate the cases when an event could not be delivered.
 * There are three cases where the event is returned to its sender.
 *	1) the case where the recipient did not register an event handler using the registerEventHandler<EventType> method
 *	2) the case where the recipient address (ActorId) is not valid (is not assigned to any actor)
 *	3) the case where the recipient receives the message but desires to send it back to the sender by throwing a ReturnToSenderException
 *	In the previous three cases, the sender is called on an onUndeliveredEvent method if the registerOnUndeliveredEvent<SomeEvent> was called for the desired event type
 */

#include <iostream>

#include "simplx.h"

using namespace std;
using namespace tredzone;

struct RegistredEvent : Actor::Event {};
struct UnregistredEvent : Actor::Event {};

class ReceiverActor : public Actor
{
public:

	// ctor
    ReceiverActor()
    {
		registerEventHandler<RegistredEvent>(*this);
	}

	void onEvent(const RegistredEvent& )
    {
		// we received a RegistredEvent but throw a ReturnToSenderException
		throw ReturnToSenderException();
	}
};

class SenderActor : public Actor
{
public:
    // ctor
    SenderActor()
    {
		const ActorId receiverActorAddress = newUnreferencedActor<ReceiverActor>();     // instantiate a ReceiverActor
		Event::Pipe pipeToReceiverActor(*this, receiverActorAddress);
		registerUndeliveredEventHandler<UnregistredEvent>(*this);
		registerUndeliveredEventHandler<RegistredEvent>(*this);

        // will trigger onUndeliveredEvent(const UnregistredEvent&) because ReceiverActor did not call regisrerEventHandler<Event>(...)
		pipeToReceiverActor.push<UnregistredEvent>();   
        
		// will trigger onUndeliveredEvent(const RegistredEvent&) because ReceiverActor throws a ReturnToSenderException from its event handler
        pipeToReceiverActor.push<RegistredEvent>();

		ActorId emptyActorId = ActorId();
		Event::Pipe	invalidDestinationPipe(*this, emptyActorId);
        
         // will trigger onUndeliveredEvent(const RegistredEvent&) because emptyActorId isn't used by an actor
		invalidDestinationPipe.push<RegistredEvent>();
	}

	void onUndeliveredEvent(const UnregistredEvent&)
    {
		cout << "SenderActor::onUndeliveredEvent(const UnregistredEvent&)" << endl;
	}

	void onUndeliveredEvent(const RegistredEvent&)
    {
		cout << "SenderActor::onUndeliveredEvent(const RegistredEvent&)" << endl;
	}
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    cout << "tutorial #6 : undelivered event management" << endl;
    
    Engine::StartSequence   startSequence;

    startSequence.addActor<SenderActor>(1);

    Engine engine(startSequence);

    sleep(2);       // (see tuto #9 for synchronous exit)
    
    return 0;
}
