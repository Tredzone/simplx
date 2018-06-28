/**
 * @file RefMapper.cpp
 * @brief per-core actor references centralized manager
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#include <unordered_map>
#include <unordered_set>

#include "trz/engine/actor.h"

#include "trz/engine/RefMapper.h"

using namespace std;
using namespace tredzone;

// implementation

namespace tredzone
{
    
class RefMapper: public IRefMapper
{
public:
    
    // ctor
    RefMapper()
    {
            
    }
    
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
        // return false;       // short circuit!
        
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
    
    unordered_map<Actor*, unordered_set<Actor*>>    m_MapToDest;
    unordered_map<Actor*, unordered_set<Actor*>>    m_MapToSource;
};

//---- INSTANTIATE -------------------------------------------------------------

// static
IRefMapper*     IRefMapper::Create(void)
{
    return new RefMapper();
    
}

} // namespace