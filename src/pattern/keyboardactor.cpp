/**
 * @file keyboardactor.cpp
 * @brief asynchronous keyboard actor service utility
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <sys/select.h>
#include <termios.h>        // termios, TCSANOW, ECHO, ICANON
#include <unistd.h>         // STDIN_FILENO

#include "simplx.h"

#include "trz/pattern/keyboardactor.h"

using namespace std;
using namespace tredzone;

// (using Scott Meyers' PIMPL idiom)

class Keyboard: public IKeyboard
{
public:
    // ctor
    Keyboard()
    {
        // set stdin attributes
        ::tcgetattr(STDIN_FILENO, &m_OldTerm);
        
        struct termios  new_term = m_OldTerm;
    
        // disable need for Enter/EOL
        new_term.c_lflag &= ~(ICANON | ECHO);          

        // change attributes immediately
        ::tcsetattr(STDIN_FILENO, TCSANOW, &new_term);
    }
    
    // dtor
    ~Keyboard()
    {
        // restore stdin attributes
        ::tcsetattr(STDIN_FILENO, TCSANOW, &m_OldTerm);
    }
    
    char    getAnyDownKey(void) override
    {
        // watch stdin (fd 0) for input
        FD_ZERO(&m_ReadFDS);
        FD_SET(STDIN_FILENO, &m_ReadFDS);
        
        struct timeval tv;
        
        tv.tv_sec = 0;
        tv.tv_usec = 0;
        
        const int    res = select(1/*highest file descriptor*/, &m_ReadFDS, NULL, NULL, &tv);
        if (res == -1)
        {   // error
            cout << "Error in select: " << endl;
            return 0;
        }
        else if (res)
        {
            if (FD_ISSET(STDIN_FILENO, &m_ReadFDS))
            {
                // process input
                const char  c = cin.get();
                
                return c;
            }
        }

        return 0;
    }
    
    struct termios  m_OldTerm;
    fd_set          m_ReadFDS;
};

//---- INSTANTIATION -----------------------------------------------------------

// static
IKeyboard*   IKeyboard::Create(void)
{
        return new Keyboard();
}

//---- Keyboard Actor ----------------------------------------------------------

    KeyboardActor::KeyboardActor()
        : m_Keyboard(IKeyboard::Create()), m_Subscriber(ActorId())
{
    registerEventHandler<KeyboardSubscribeEvent>(*this);
}

//---- Subscribe event handler -------------------------------------------------

void    KeyboardActor::onEvent(const KeyboardSubscribeEvent &e)
{
    if (m_Subscriber != ActorId())
    {
        // error - already had a subscriber
        throw ReturnToSenderException();
    }
    
    m_Subscriber = e.getSourceActorId();
    
    // restart callbacks now that has a subscriber
    registerCallback(*this);
}

//---- Unsubscribe event handler -----------------------------------------------

void    KeyboardActor::onEvent(const KeyboardUnsubscribeEvent &e)
{
    if (m_Subscriber != e.getSourceActorId())
    {
        // error - had a different subscriber
        throw ReturnToSenderException();
    }
    
    // (will suspend callbacks until has a new subscriber)
    m_Subscriber = ActorId();    
}

//---- polling callback --------------------------------------------------------

void    KeyboardActor::onCallback(void)
{
    if (m_Subscriber == ActorId())      return;         // has no current subscriber -> SUSPEND callbacks until has a new subscriber
    
    // re-register
    registerCallback(*this);

    const char  c = m_Keyboard->getAnyDownKey();
    if (!c)     return;
    
    // got a key, send to subscriber
    Event::Pipe pipe(*this, m_Subscriber);
    pipe.push<KeyboardEvent>(c);
}

// nada mas
