
# Tutorial #8 - Ping Pong

This tutorial shows how Actors are instantiated and how they communicate with each other.
A PongActor is instantiated as a service on CPU core #2, then a PingActor is instantiated on CPU core #1.
A TravelLogEvent is sent from Ping to Pong and back to Ping, recording its location at each stop.

The workflow terminates after a hardcoded number for seconds; see tutorial #9 for a better and more elaborate exit procedure.


