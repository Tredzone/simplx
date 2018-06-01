# Tutorial #6 - Undelivered event management

Sometimes, when an Event is sent from one (sender) actor to another (recipient) actor, it may not arrive properly at its destination. This may happen for various reasons.
In this tutorial you'll learn why this may happen, and when it does, how the sender is notified

## Cases where an Event may be not delivered

- Invalid destination Actor address (ActorId)

As you know now, to send an Event you have to create a Pipe by passing the ActorId of the destination actor, if this ActorId is invalid (does not correspond to any actor) the event will not be delivered.

- When the destination actor did not call `registerEventHandler()`

To receive an Event, the Actor must call the `registerEventHandler` mathod with the expected Event type, and implement the `void onEvent(const MyEvent&)` method for the same type of event.

```c++
registerEventHandler<MyEvent>(*this);

void onEvent(const MyEvent&)
{
}
```

If an actor didn't previously call `registerEventHandler` it won't be able to receive such Events sent to him.

- when the recipient Actor forces the return of a given Event to its sender

The recipient Actor may decide to return the Event to its sender by throwing a ReturnToSenderException.
 
```cpp
void onEvent(const MyEvent&)
{
    throw ReturnToSenderException();
}
```

## how the sender is notified
In order to be notified of the undelivered message, the sender must:
- Call `registerUndeliveredEventHandler`

```c++
MyActor()
{
	registerUndeliveredEventHandler<SomeEvent>(*this);
}
```

- Implement `onUndeliveredEvent` for a every event type for which such a notification is wanted/expected

```c++
void onUndeliveredEvent(const SomeEvent&)
{
	// do something
}
```
