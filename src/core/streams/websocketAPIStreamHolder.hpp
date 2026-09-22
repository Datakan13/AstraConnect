#pragma once
#include "net/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include <simdjson/simdjson.h>
#include "core/protocol/requestParameter.hpp"
#include "core/types/status.hpp"
#include "utils/net/exponentialBackOff.hpp"
#include "utils/crypto/hmacSha256.hpp"
#include <chrono>

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
    boost::beast::flat_buffer buffer;
    ConnectionStatus connectionStatus;
    AstraConnect::Utils::BackoffPolicy backoffPolicy;
    
    ConnectionStatus setupConnection() {
        try {
            auto const results = resolver.resolve(host, port);
            ctx.set_default_verify_paths();
            net::connect(ws.next_layer().next_layer(), results);

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
            ws.text(true);
            return ConnectionStatus::SUCCESS;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            return ConnectionStatus::FAIL;
        }    
    }
    
    ConnectionStatus exponentialBackOff() {
        return AstraConnect::Utils::exponentialBackOff(
            [this]{ return setupConnection(); },
            backoffPolicy);
    }

    public:
    // Returns true if the connection is alive
    // LIMITATION: It can only have the current state of the connection if it's been newly constructed or had a request through it
    bool connectionAlive() {
        return connectionStatus == ConnectionStatus::SUCCESS;
    }

    // Will return a StreamData struct with components
    // status: ConnectionStatus object 
    // string: simdjson::padded_string object
    // ec: boost::beast::error_code
    // the returned StreamData should be used as if(streamData) string valid otherwise invalid check ec
    StreamData sendRequest(std::string& request) {
        StreamData data;
        beast::error_code ec;

        ws.write(net::buffer(request),ec);
        if(ec) {
            data.ec = ec;
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }
        ws.read(buffer,ec);
        if(ec) {
            data.ec = ec;
            buffer.consume(buffer.size());
            connectionStatus = ConnectionStatus::FAIL;
            return data;
        }

        data.string = simdjson::padded_string(
            boost::beast::buffers_to_string(buffer.data())
        );
        buffer.consume(buffer.size());
        data.status = ConnectionStatus::SUCCESS;
        return data;
    }

    std::string getHMAC(std::string& data) {
        return hmac_sha256(privateKey,data);
    }

    // WARNING: the caller needs to check if the connection is alive with connectionAlive() since it is non-throwing
    WebsocketAPIStreamHolder(std::string host_, std::string& target_) : host(host_), target(target_),  ctx((net::ssl::context::tls_client)), ws(ioc,ctx), resolver(ioc) {
        APIKey = std::getenv("API_KEY");
        privateKey = std::getenv("PRIVATE_KEY");
        connectionStatus = setupConnection();
        if(connectionStatus != ConnectionStatus::SUCCESS) connectionStatus = exponentialBackOff();
    };

};