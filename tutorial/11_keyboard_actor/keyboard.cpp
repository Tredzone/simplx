/*
 * keyboard.cpp
 *
 * Please see accompanying README.
 */

#include <iostream>
#include <iomanip>
#include <string>
#include <fstream>
#include <memory>

#include "simplx.h"

#include "trz/pattern/waitcondition.h"
#include "trz/pattern/keyboardactor.h"

using namespace std;
using namespace tredzone;

//---- Keyboard Listener -------------------------------------------------------

class CommandListener: public Actor
{
public:
    // ctor
    CommandListener(shared_ptr<IWaitCondition> wait_cond)
        : m_WaitCond(wait_cond)
    {
        // subscribe to keyboard event
        const ActorId keyboard_service = getEngine().getServiceIndex().getServiceActorId<KeyboardActor::ServiceTag>();		
        Event::Pipe pipe(*this, keyboard_service);
        pipe.push<KeyboardSubscribeEvent>();
        
        registerEventHandler<KeyboardEvent>(*this);
    }
    
    void    onEvent(const KeyboardEvent &e)
    {
        const char  c = e.m_C;
        
        switch (c)
        {   case 'q':
        
                cout << "quitting..." << endl;
                
                m_WaitCond->notify();
                requestDestroy();
                break;
            
            case '1':
            
                cout << "key 1" << endl;
                break;
            
            case '2':
            
                cout << "key 2" << endl;
                break;
            
            default:
            
                cout << "unhandled key down = " << c << endl;
                break;
        }
    }
    
private:
    
    shared_ptr<IWaitCondition>     m_WaitCond;
};

//---- MAIN -------------------------------------------------------------------

int main()
{
    cout << "tutorial #11 : keyboard" << endl;
    
    shared_ptr<IWaitCondition>  wait_condition(IWaitCondition::Create());
    
    Engine::StartSequence	start_seq;
	
    start_seq.addServiceActor<KeyboardActor::ServiceTag, KeyboardActor>(0);
    start_seq.addActor<CommandListener>(0, wait_condition);
    
	Engine	engine(start_seq);
	
    wait_condition->wait();
    
    return 0;
}
