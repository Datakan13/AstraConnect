#pragma once
#include "boost_include_helpers/beast_boost_includes.hpp"

namespace boost {
    namespace beast {
        template<>
        struct is_sync_read_stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
            : std::true_type
        {};
    
        template<>
        struct is_sync_write_stream<boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
            : std::true_type
        {};
    }
    }
    
    

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
namespace ssl = net::ssl;
using namespace boost::asio;

using tcp = net::ip::tcp;