/**
 * @file RefMapper.cpp
 * @brief per-core actor references centralized manager
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <queue>

#include "trz/engine/actor.h"
#include "trz/engine/internal/node.h"
#include "trz/engine/internal/RefMapper.h"

using namespace std;
using namespace tredzone;

// implementation

namespace tredzone
{
    
class RefMapper: public IRefMapper
{
public:
    
    // ctor
    RefMapper(const AsyncNode &nod)
        : m_Node(nod), m_Id(s_Id++)
    {
        m_LiveActorSet.clear();
        
        (void)m_Node;
    }
    
    void    onActorAdded(const Actor *actor) override
    {
        assert(actor);
        
        // dispatch existing
        DispatchQueue();
        
        // enqueue new
        m_ActorQueue.push_back(actor);
    }
    
    void    onActorRemoved(const Actor *actor) override
    {
        assert(actor);
        
        // dispatch existing
        DispatchQueue();
        
        assert(m_LiveActorSet.count(actor));
        
        const size_t    n_erased = m_LiveActorSet.erase(actor);
        assert(n_erased == 1);
        
        (void)n_erased;
    }
    
    size_t  getNumActors(void) const override
    {
        return m_LiveActorSet.size() + m_ActorQueue.size();
    }
    
    //----------------------------------------------------------------------
    
    bool    isDependant(const Actor *org, const Actor *dest) override
    {
        (void)org;
        (void)dest;
        
        return false;
    }
    
    void    AddRef(const Actor *org, const Actor *dest) override
    {
        (void)org;
        (void)dest;
    }
    
    void    RemoveRef(const Actor *org, const Actor *dest) override
    {
        (void)org;
        (void)dest;
    }
    
    bool    recursiveFind(const Actor &dest, const Actor &org) noexcept override
    {
        #if (TREDZONE_CHECK_CYCLICAL_REFS == 0)
            return false;       // short circuit!
        #endif
        
        return recursiveFind_LL(dest, org);
    }
       
private:

    bool    recursiveFind_LL(const Actor &dest, const Actor &org) noexcept
    {
        if (&dest == &org)                              return true;
       
        for (auto &it : org.m_ReferenceToChain)
        {
               const Actor &new_org = *it.getReferencedActor();
               
               if (recursiveFind_LL(dest, new_org))    return true;        // found!
        }

        return false;   // not found
    }
    
    void    DumpActor(const Actor *actor) const
    {
        assert(actor);
        
        #ifdef DTOR_DEBUG
            cout << actor->getActorId() << "  " << cppDemangledTypeInfoName(typeid(*actor)) << endl;
        #else
            (void)actor;
        #endif
    }
    
    void    dumpAllActors(void) const override
    {
        /*
        cout << "dumpAllActors" << endl;
        
        for (auto *actor : m_LiveActorSet)
        {
            DumpActor(actor);
        }
        */
    }
    
    void    DispatchQueue(void)
    {
        while (!m_ActorQueue.empty())
        {
            const Actor *actor = m_ActorQueue.front();
            
            #ifdef DTOR_DEBUG
                cout << "dispatching (node " << m_Id << ") ";
                DumpActor(actor);
            #endif
            
            m_ActorQueue.pop_front();
            
            assert(!m_LiveActorSet.count(actor));
            
            m_LiveActorSet.insert(actor);
        }
    }
    
    static int          s_Id;
    const AsyncNode     &m_Node;
    const int           m_Id;
    
    set<const Actor*>   m_LiveActorSet;
    list<const Actor*>  m_ActorQueue;
};

// static
int          RefMapper::s_Id = 0;

//---- INSTANTIATE -------------------------------------------------------------

// static
IRefMapper*     IRefMapper::Create(const AsyncNode &nod)
{
    return new RefMapper(nod);
    
}

} // namespace