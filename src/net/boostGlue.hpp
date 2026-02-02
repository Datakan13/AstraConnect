#pragma once
#include "net/boostPrelude.hpp"

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

using tcp = net::ip::tcp;