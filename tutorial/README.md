# Simplx tutorials

In this directory you'll find the following tutorials, sorted by inceasing complexity:

1. [hello actor](./01_hello_actor/README.md) - Introduces the Framework by creating a new Actor
2. [hello world](./02_hello_world/README.md) - Define your own simple Actor and a Callback
3. [printer actor starter](./03_printer_actor_starter/README.md) - Create two actors that interact via message passing
4. [printer actor service](./04_printer_actor_service/README.md) - As example #3 but use the ServiceTag to register in the service index
5. [multi-callback](./05_multi_callback/README.md) - Use two different callback functions with a single Actor
6. [undelivered event management](./06_undelivered_event_management/README.md) - why message passing/event delivery may fail and how to handle it.
7. [referenced and unreferenced actors](./07_referenced_unreferenced_actor/README.md) - Introduces referenced actors and the same core invocation optimisation pattern. 
8. [ping pong](./08_pingpong/README.md) - Brings together earlier examples to create a two actor system with service discovery and two-way (asynchronous) communication. 
9. [asynchronous exit](./09_sync_exit/README.md) - Demonstrates a controlled exit using requestDestroy()
10. [timer](./10_timer/README.md) - Demonstrates a periodic timer using the timer_proxy class.
11. [asynchronous keyboard polling](./11_keyboard_actor/README.md) - Demonstrates the built-in keyboard actor, handling keypress events
12. [asynchronous tcp/http client & server](./12_connector/README.md) - Demonstrates TCP and HTTP connector built-in actors
13. [cross thread bus](./13_cross_thread_bus/README.md) - Demonstrates the integration of cooperative Actor with external services that are not so well-mannered. Can be used for integrating with thirdparty systems with their own eventloops and/or blocking services.

Each folder contains its own README file with further details.
