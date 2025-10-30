#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include "simdjson.h"
#include "dataTypes/requestParameter.hpp"
#include "../../error.hpp"
#include "hmac_sha256.hpp"

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
    ConnectionStatus setupConnection() {
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
            return ConnectionStatus::SUCCESS;
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            return ConnectionStatus::FAIL;
        }    
    }
    
    ConnectionStatus exponentialBackOff() {
        const int delay = 500;
        const int increasePerTry = 2;
        const int maxTryCount = 20;
        const int maxDelay = static_cast<int>(delay * std::pow(increasePerTry, maxTryCount));
        chrono::milliseconds backoff(delay);
        ConnectionStatus status;
        for(;;) {
            std::this_thread::sleep_for(backoff);
            status = setupConnection();
            if(status == ConnectionStatus::SUCCESS) {
                return ConnectionStatus::SUCCESS;
            }
            backoff *= increasePerTry;
            if(backoff.count() >= maxDelay) {
                return ConnectionStatus::FAIL;
            }
        }
        
    }

    public:
    simdjson::padded_string sendRequest(std::string& request) {
        ws.write(net::buffer(request));
        ws.read(buffer);
        auto json = simdjson::padded_string(
                boost::beast::buffers_to_string(buffer.data())
            );
            buffer.consume(buffer.size());
        return json;
    }

    std::string getHMAC(std::string& data) {
        return hmac_sha256(privateKey,data);
    }

    WebsocketAPIStreamHolder(std::string host_, std::string& target_) : host(host_), target(target_),  ctx((net::ssl::context::sslv23)), ws(ioc,ctx), resolver(ioc) {
        APIKey = std::getenv("API_KEY");
        privateKey = std::getenv("PRIVATE_KEY");
        ConnectionStatus status = setupConnection();
        if(status != ConnectionStatus::SUCCESS) {
            status = exponentialBackOff();
            if(status == ConnectionStatus::FAIL) {
                std::string error = "Connection to websocket failed for host: " + host_ + target_;
                throw std::runtime_error(error);
            } 
        }
    };

};