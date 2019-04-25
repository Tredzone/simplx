
# Tutorial #1 - Hello Actor

In this tutorial you'll learn to

- add headers and tredzone namespace in order to use Simplx<br/>
- define a simple actor<br/>
- instantiate the actor by inserting it in the StartSequence and then starting the engine
- block the main thread until user types the Enter key
- build using cmake, make or a g++ onle-liner<br/>


## add Simplx header and use tredzone namespace

The API is defined in the topmost header "simplx.h"; including it should be all that is required.<br/>
To use shorter symbol names, it's convenient to use the *tredzone namespace*

```c++
#include "simplx.h"

using namespace tredzone;
using namespace std;
```

## define a simple actor

To define an actor, just inherit from the *Actor* class
```c++
class HelloActor: public Actor
...
```

## instantiate the actor by inserting it in the StartSequence and then starting the engine

First, a *StartSequence* must be instantiated.<br/>
Next, we add actor classes into the *StartSequence*, specifying the core on which it must be run (here the core number 1).<br/>
Finally, we instantiate actors that were added to the *StartSequence* by creating an Engine. Here we wait for suser input since we want the Engine not to destroy immediately after instantiation.

```c++
Engine::StartSequence startSequence;
startSequence.addActor<HelloActor>(1);
Engine engine(startSequence);
```

## block main thread until user presses the enter key

```c++
 cout << "Press enter to exit...";
 cin.get();
```

## build

### cmake

```sh
mkdir build && cd build
cmake ..
make
```


### make

```sh
make
```


### g++ one-liner

While using CMake to build largish projects is easier, here is the g++ command to build this small tutorial:

```bash
g++ -std=c++11 -pthread -I ../../include/ ../../src/engine/actor.cpp ../../src/engine/e2e_stub.cpp ../../src/engine/engine.cpp ../../src/engine/node.cpp ../../src/engine/RefMapper.cpp ../../src/engine/parallel/parallel_xplat.cpp ../../src/engine/linux/platform_linux.cpp ../../src/engine/linux/platform_gcc.cpp hello_actor.cpp -o hello_actor.bin

```
