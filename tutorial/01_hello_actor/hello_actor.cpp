#include <iostream>

#include "simplx.h"

using namespace tredzone;
using namespace std;

class HelloActor : public Actor
{
public:
    // ctor
    HelloActor()
    {
        cout << "Actor created" << endl;
    }
};

//---- MAIN --------------------------------------------------------------------

int main()
{
    cout << "tutorial #1 : hello actor" << endl;
    
    Engine::StartSequence startSequence;	// container storing Actors to run upon engine start
    startSequence.addActor<HelloActor>(1);	// a HelloActor will be instantiated on CPU core #1
    
    Engine	engine(startSequence);
    
    cout << "Press enter to exit...";
    cin.get();

    return 0;
}
