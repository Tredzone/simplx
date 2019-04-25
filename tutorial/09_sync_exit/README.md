
# Tutorial #9 - synchronous exit

This tutorial shows how Actors are instantiated and how they communicate with each other.
A PongActor is instantiated as a service on CPU core #2, then a PingActor is instantiated on CPU core #1.
A TravelLogEvent is sent from Ping to Pong and back to Ping, recording its location at each stop.

This tutorial also shows how to terminate the workflow deterministically by overriding PingActor's `onDestroyRequest()` method and either accepting the destruction request with `acceptDestroy()`, if the workflow is done, or postponing the destruction request, if work is still ongoing, and requesting the destruction be attempted again at the next event loop iteration by calling `requestDestroy()`.
