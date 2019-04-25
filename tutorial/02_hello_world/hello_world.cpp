#include <iostream>
#include <simplx.h>

using namespace tredzone;
using namespace std;

/* HelloWorldActor inherits both Actor and Actor::Callback
* thereby it can use the callback functionality
**/
class HelloWorldActor : public Actor, public Actor::Callback
{
public:

    // CTOR
    HelloWorldActor()
    {
        // register call so will be called at next event loop iteration
        registerCallback(*this);
    }

    void onCallback()
    {
        cout << "Hello World" << endl;
    }
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    cout << "tutorial #2" << endl;
    
    Engine::StartSequence startSequence;
    startSequence.addActor<HelloWorldActor>(1);

    Engine engine(startSequence); // Start the the engine

    cout << "Press enter to exit...";       // (see tuto #9 for synchronous exit)
    cin.get();

    return 0;
}
