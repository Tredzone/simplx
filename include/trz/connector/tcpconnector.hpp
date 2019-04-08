#pragma once

#include "trz/connector/tcp/client/client.hpp"
#include "trz/connector/tcp/common/networkcalls.hpp"
#include "trz/connector/tcp/common/network.hpp"
#include "trz/connector/tcp/server/server.hpp"
#include "simplx.h"

using tredzone::Engine;
using tredzone::connector::tcp::TcpClient;
using tredzone::connector::tcp::NetworkCalls;
using tredzone::connector::tcp::Network;
using tredzone::connector::tcp::TcpServer;

using fd_t = int64_t;