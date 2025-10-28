#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include "simdjson.h"
#include "threadSafeParser.hpp"
#include <bitset>
#include <AstraLib/AstraLib.hpp>
#include <type_traits>
#include <openssl/hmac.h>
#include "requestParameter.hpp"

inline std::string hmac_sha256(const std::string& key, std::string data) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    data.erase(0,1);
    data.pop_back();
    for (auto& c : data) {
    if (c == ',')  c = '&';
    else if (c == ':') c = '=';
    else if (c == '"') c = '\0';
    else if (c == ' ') c = '\0';
    }
    data.erase(std::remove(data.begin(), data.end(), '\0'), data.end());
    std::cout << "data: " << data << std::endl;

    HMAC(EVP_sha256(),
         key.data(), static_cast<int>(key.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &len);

    static constexpr char hexmap[] = "0123456789abcdef";
    std::string out;
    out.resize(len * 2);

    for (unsigned int i = 0; i < len; ++i) {
        unsigned char c = digest[i];
        out[2 * i]     = hexmap[c >> 4];
        out[2 * i + 1] = hexmap[c & 0xF];
    }
    return out;
}



class StreamHolder {
    net::io_context& ioc;
    net::ssl::context& ctx;
    tcp::resolver resolver;
    std::string port = "443";
    std::string host;
    const int version = 11;
    beast::ssl_stream<tcp::socket> stream;
    beast::flat_buffer buffer;
    http::response<http::dynamic_body> res;
    AstraLib::Atomic::Spinlock lock;

    // establishes a connection to the main host
    void establishConnection(beast::ssl_stream<tcp::socket>& stream,const std::string host) {
        ctx.set_default_verify_paths();
        stream.set_verify_mode(boost::asio::ssl::verify_peer);
        stream.set_verify_callback(boost::asio::ssl::rfc2818_verification(host));

        // Resolve and connect
        auto const result = resolver.resolve(host, port);
        net::connect(stream.next_layer(), result);

        // Set SNI 
        if(!SSL_set_tlsext_host_name(stream.native_handle(), host.c_str()))
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()),
                "Failed to set SNI Host Name");

        // Perform TLS handshake
        stream.handshake(ssl::stream_base::client);
    }

    public:


    //Returns a simdjson::padded_string that can be iterated
    auto sendRequest(const std::string& target, http::verb method = http::verb::get, bool keepAlive = true) {
        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};
        req.set(http::field::host, host);
        req.set(http::field::user_agent, "APIManager/1.0");
        if (keepAlive) {
            req.set(http::field::connection, "keep-alive");
        }
        http::write(stream, req);
        http::read(stream, buffer, res);
        auto json = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        buffer.consume(buffer.size());
        res.body().clear(); 
        res.clear();
        return json;
    }
    

    template<typename... Args>
    auto sendRequest(const std::string& target, http::verb method = http::verb::get,
        bool keepAlive = true, bool needBody = false,
        RequestParameter<Args>... requestParameters) {

        AstraLib::Atomic::SpinlockGuard guard(lock);
        http::request<http::string_body> req{method, target, version};

        req.set(http::field::host, host);
        req.set(http::field::user_agent, "APIManager/1.0");
        if (keepAlive) {
            req.set(http::field::connection, "keep-alive");
        }

        if(needBody) {
            std::string test = makeRequestFromRequestParameters(std::forward<RequestParameter<Args>>(requestParameters)...);
            req.body() = test;
            req.prepare_payload();
        } else {
            ((req.set(requestParameters.requestField, requestParameters.request)), ...);
        }
       
        
        
        http::write(stream, req);
        http::read(stream, buffer, res);
        auto json = simdjson::padded_string(
            boost::beast::buffers_to_string(res.body().data())
        );
        buffer.consume(buffer.size());
        res.body().clear(); 
        res.clear();
        return json;
    }

    StreamHolder(net::io_context& ioc_, net::ssl::context& ctx_, std::string host_) : 
    ioc(ioc_),ctx(ctx_) 
    ,stream(ioc_,ctx_) , host(host_),resolver(ioc_){
        establishConnection(stream,host);
    }
};

// The first template is the data you want to keep e.g. TradeEvent 
// The second template is your data construction function which must return your data type 
// The second template should also accept simdjson::padded_string
//e.g. [simdjson::padded_string json](){  TradeEvent event = json; return event; }
template<typename Data,typename Func>
class WebsocketStreamHolder {
    net::io_context ioc;
    net::ssl::context ctx;
    tcp::resolver resolver;
    std::string target;
    std::string host;
    std::string port = "443";
    beast::websocket::stream<ssl::stream<tcp::socket>> ws;
    AstraLib::Atomic::PaddedAtomic<bool> running = true;
    beast::flat_buffer buffer;
    Func func;
    ThreadSafeParser& parserWrapper;
    std::thread thread;
    void setupConnection() {
        try {
            auto const results = resolver.resolve(host, port);
            ctx.set_default_verify_paths();
            net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

            // Set SNI Hostname (required by many TLS servers)
            if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                        net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            
            // Perform the SSL handshake
            ws.next_layer().handshake(ssl::stream_base::client);

            // Now perform the WebSocket handshake
            boost::beast::websocket::response_type response;
            ws.handshake(response,host, target);
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
        }
    }

    public:
    // A variable that becomes true when a Data has been queued 
    // No release mechanism for making it false it is user's responsibility to release
    AstraLib::Atomic::PaddedAtomic<bool> controlVariable = false;

    AstraLib::Buffers::AtomicRingBuffer<Data,1024> bufferOut;

    auto getLatestMessage() {
        try {
            ws.read(buffer);
            auto json = simdjson::padded_string(
                boost::beast::buffers_to_string(buffer.data())
            );
            buffer.consume(buffer.size());
            return json;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            auto string = simdjson::padded_string();
            return string;
        }
        
    }

    void executionLoop() {
        setupConnection();
        Data data;
        while(running.value.load(std::memory_order_acquire)) {
            auto json = getLatestMessage();
            ThreadSafeParserRAII parser(parserWrapper);

            data = func(json,parser.parser);
            bufferOut.noMoveEnqueue(data);
            controlVariable.value.store(true,std::memory_order_release);
        }
    }

    WebsocketStreamHolder(Func func_, 
    std::string host_, std::string target_,ThreadSafeParser& parser_) : 
    resolver(ioc), ws(ioc,ctx),
    target(target_) ,host(host_), 
    ctx(boost::asio::ssl::context::sslv23), parserWrapper(parser_), 
    func(std::move(func_)), thread(&WebsocketStreamHolder::executionLoop,this) {        
    
    }

    ~WebsocketStreamHolder() {
        running.value.store(false, std::memory_order_release);
        thread.join();
    }

};

class WebsocketAPIStreamHolder {
    net::io_context ioc;
    net::ssl::context ctx;
    std::string host;
    std::string target;
    beast::websocket::stream<ssl::stream<tcp::socket>> ws;
    tcp::resolver resolver;
    std::string port = "443";
    std::string APIKey;
    std::string privateKey;
    std::thread* reader;
    AstraLib::Atomic::PaddedAtomic<bool> running = true;
    void setupConnection() {
        try {
            auto const results = resolver.resolve(host, port);
            ctx.set_default_verify_paths();
            net::connect(ws.next_layer().next_layer(), results.begin(), results.end());

            // Set SNI Hostname (required by many TLS servers)
            if(!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                        net::error::get_ssl_category()};
                throw beast::system_error{ec};
            }

            
            // Perform the SSL handshake
            ws.next_layer().handshake(ssl::stream_base::client);
            std::cout << "Setting up connection to: " << host << target << std::endl;
            // Now perform the WebSocket handshake
            boost::beast::websocket::response_type response;
            ws.handshake(response,host, target);
            ws.text(true);
            std::cout << response << std::endl;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
        }    
    }

    void readLoop() {
        boost::beast::flat_buffer buffer;
        while (running.value.load(std::memory_order_acquire)) {
            boost::system::error_code ec;
            ws.read(buffer, ec);
            if (ec) {
                if (ec == boost::beast::websocket::error::closed) {
                    std::cout << "[WebSocket closed]" << std::endl;
                } else {
                    std::cout << "[Read error] " << ec.message() << std::endl;
                }
                break;
            }
            std::cout << beast::buffers_to_string(buffer.data()) << std::endl;
            buffer.consume(buffer.size());
        }
    }


    public:


    void sendRequest(std::string& request) {
        ws.write(net::buffer(request));
    }

    std::string getHMAC(std::string& data) {
        return hmac_sha256(privateKey,data);
    }

    WebsocketAPIStreamHolder(std::string host_, std::string& target_) : host(host_), target(target_),  ctx((net::ssl::context::sslv23)), ws(ioc,ctx), resolver(ioc) {
        APIKey = std::getenv("API_KEY");
        privateKey = std::getenv("PRIVATE_KEY");
        setupConnection();
        reader = new std::thread([this]() { readLoop(); });

    };

    ~WebsocketAPIStreamHolder() {
        running.value.store(false,std::memory_order_release);
        reader->join();
    };
};
