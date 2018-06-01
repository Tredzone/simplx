
# Tutorial #3 - Printer Actor using a starter Actor

In this tutorial you will learn to start multiple unreferenced Actors and how to send Events between these Actors.

The following Simplx concepts are introduced:

- Actor::newUnreferencedActor method
- Actor::registerEventHandler method
- Pipe class
- Pipe::push method
- Event class
- onEvent event handler
 


## Actor methods
This tutorial makes use of several Actor methods:

### newUnreferencedActor
This method instantiates a given Actor on the same cpu core as the caller. A unique `ActorId` corresponding to the newly created Actor instance is then returned.

As can be seen from the following code snippet, the `newUnreferencedActor` method is overloaded to accept an `_ActorInit` argument. This allows you to pass an argument to the `Actor`'s constructor.

```c++
template<class _Actor> 
const Actor::ActorId& Actor::newUnreferencedActor();

template<class _Actor, class _ActorInit>
const Actor::ActorId& Actor::newUnreferencedActor(const _ActorInit& init)
```

**Note**: A `newReferencedActor` method can also be used to instantiate an Actor on the same core as the caller. This method will be explained in a future tutorial.


## registerEventHandler
This method is used to register a callback method that will be called when an `Event` of said type is expected to be received by the Actor. The matching method must be implemented for the application to compile:

 `template<typename _Event> void onEvent(const _Event&)`


## Pipe
A pipe is a uni-directionnal communication channel between 2 Actors. In this tutorial a pipe is used between a WriterActor and a PrinterActor.
As pipes are uni-directionnal, a source Actor and a destination ActorId must be specified.

You instantiate a pipe with the following constructor:

`Pipe(Actor& sourceActor, const ActorId& destinationActorId)`

Once instantiated, a pipe can be used to send an Event to a destination Actor using the `push()` method.

## Event
For an Event to be sent to an Actor, it must derive from the the `Event` class.

The following code snippet shows a PrintEvent class holding an std::string payload.

```c++
struct PrintEvent : Actor::Event
{
	PrintEvent(const std::string& message) : message(message)
	{
	}
	const std::string message;
};
```

The above PrintEvent can be sent to an Actor by using an existing pipe's `push` method. The arguments passed to the method correspond to the Event's constructor.

```c++
pipe.push<PrintEvent>("Hello, World!");
```


