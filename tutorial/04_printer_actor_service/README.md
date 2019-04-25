# Tutorial #4 - Printer Actor using a service Actor

In this tutorial you'll learn to start multiple Actors and how to send Events between these Actors.
Unlike in the previous tutorial, however, the Printer Actor will be started as a Service.

The following new concepts are introduced:

- StartSequence::addServiceActor() method
- Service and ServiceIndex


## StartSequence

The StartSequence is used to configure the initial Actors of the `Engine` -- right before switching to asynchronous execution.

Methods used in this tutorial are the `addActor()` and `addServiceActor()`, which are used to add Actors to the StartSequence.
Actors in the StartSequence will be **instantiated by the Engine** in the **same order as they are added**.


## Service and Service index

The `addServiceActor` method takes two template parameters: `_Service` and `_Actor`, in addition to the CPU `CoreId` argument.

```c++
template<class _Service, class _Actor>
void Engine::StartSequence::addServiceActor(CoreId coreId);
```

The `_Service` template corresponds to the unique tag with which the Actor will be indexed in the `ServiceIndex`. Any Actor can retrieve the `ActorId` corresponding to its `_Service` tag by a request to the `ServiceIndex`. The following code snippet shows how to query the ServiceIndex.

```c++
const ActorId printerActorId = getEngine().getServiceIndex().getServiceActorId<PrinterActor::Service>();
```


