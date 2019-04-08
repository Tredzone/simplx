#pragma once

#include "simplx.h"

namespace tredzone
{

class ServerSideSuccessEvent : public Actor::Event
{
};

class ClientSideSuccessEvent : public Actor::Event
{
};

class ServerSideFaillureEvent : public Actor::Event
{
};

class ClientSideFaillureEvent : public Actor::Event
{
};

struct ValidatorService : public Service
{
}; // #include "validatoractor.h"

} // namespace tredzone