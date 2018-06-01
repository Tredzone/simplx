# Tutorial #7 - Referenced and Unreferenced Actor

In this tutorial you'll learn the difference bewteen the `newUnreferencedActor` and `newReferencedActor` method. 

The following new concepts will be introduced:

- Actor::newUnreferencedActor()
- Actor::newReferencedActor()


## Actor methods
Two methods are available to instantiate an Actor from within another Actor: `newUnreferencedActor()` (used in previous tutorials) and `newReferencedActor()`.

### newUnreferencedActor()
As seen in previous tutorials, `newUnreferencedActor()` instantiates a new Actor on the same CPU core as the calling Actor. This method returns a unique `ActorId` corresponding to the newly created Actor instance.

### newReferencedActor()
Just as with `newUnreferencedActor()`, `newReferencedActor()` instantiates an Actor on the same CPU core as the caller Actor. The return type if different, however, here an `ActorReference` is returned. This object corresponds to an Actor reference.

While an Actor has an instance of a `ActorReference` object, the referenced Actor will not be able to terminate (the destructor will not be called). Hence, an `ActorReference` is always valid.

This tutorial makes use of the `ActorReference` to directly call a method from the referenced actor (PrinterActor). This bypasses having to send an Event through a pipe and the waiting for the `EventLoop` to dispatch said Event, which is obviously faster (when in the same CPU core).

```c++
ActorReference<PrinterActor> printerActorRef = newReferencedActor<PrinterActor>();
printerActorRef->printMessage("Hello using printerActorRef", getActorId());
```
