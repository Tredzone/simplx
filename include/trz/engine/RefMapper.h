/**
 * @file RefMapper.h
 * @brief per-core actor references centralized manager
 * @copyright 2013-2018 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

#pragma once

namespace tredzone
{
    class Actor;

class IRefMapper
{
public:
        
        virtual ~IRefMapper() = default;
        
        virtual bool    isDependant(const Actor *org, const Actor *dest) = 0;
        virtual void    AddRef(const Actor *org, const Actor *dest) = 0;
        virtual void    RemoveRef(const Actor *org, const Actor *dest) = 0;
        
        virtual bool    recursiveFind(const Actor &dest, const Actor &org) = 0;
        
        static
        IRefMapper*     Create(void);
};

} // namespace