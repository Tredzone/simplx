/**
 * @author Valerian Vives <valerian.vives@tredzone.com>
 * @file tcpconnector.hpp
 * @brief tcp connector top-level header
 * @copyright 2013-2019 Tredzone (www.tredzone.com). All rights reserved.
 * Please see accompanying LICENSE file for licensing terms.
 */

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