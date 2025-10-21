#pragma once
#define BOOST_ASIO_HAS_CO_AWAIT

#include <boost/asio/ssl/stream.hpp>
#include <boost/asio/ssl/error.hpp> 
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <coroutine>
#include <boost/asio/io_context.hpp>