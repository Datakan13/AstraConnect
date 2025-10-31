#pragma once
#include "boost_include_helpers/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include "simdjson.h"
#include "dataModule/dataTypes/requestParameter.hpp"
#include "dataModule/threadSafeParser.hpp"

// error handling DONE

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
    AstraLib::Atomic::PaddedAtomic<bool> connectionAlive = false;
    AstraLib::Atomic::PaddedAtomic<bool> connecting = true;
    beast::flat_buffer buffer;
    Func func;
    ThreadSafeParser& parserWrapper;
    std::thread thread;
    ConnectionStatus setupConnection() {
        connecting.value.store(true,std::memory_order_release);
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
            connectionAlive.value.store(true,std::memory_order_release);
            connecting.value.store(false,std::memory_order_release);
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
    // A variable that becomes true when a Data has been queued 
    // No release mechanism for making it false it is user's responsibility to release
    AstraLib::Atomic::PaddedAtomic<int> controlVariable = 0;

    AstraLib::Buffers::AtomicRingBuffer<Data,1024> bufferOut;

    auto getLatestMessage() {
        try {
            beast::error_code ec;

            if(connectionAlive.value.load(std::memory_order_acquire)){
                ws.read(buffer, ec);
            } else {
                return simdjson::padded_string{};
            }
            if(ec) return simdjson::padded_string{}; 
            auto json = simdjson::padded_string(
                boost::beast::buffers_to_string(buffer.data())
            );
            buffer.consume(buffer.size());
            return json;
        } catch (const boost::beast::system_error& e) {
            if (e.code() == boost::beast::websocket::error::closed) {
                connectionAlive.value.store(false,std::memory_order_release);
            }
            return simdjson::padded_string{};
        }catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            return simdjson::padded_string{};
        } 
        
    }


    ConnectionStatus getStatus() {
        while(connecting.value.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        std::cout << running.value.load(std::memory_order_acquire) << " " << connectionAlive.value.load(std::memory_order_acquire) << std::endl;
        if(!(running.value.load(std::memory_order_acquire)) || !(connectionAlive.value.load(std::memory_order_acquire)) ) return ConnectionStatus::FAIL;
        return ConnectionStatus::SUCCESS;
    }

    bool closeConnection() {
        boost::system::error_code ec;
        if(connectionAlive.value.load(std::memory_order_acquire)) connectionAlive.value.store(false,std::memory_order_release);
        if(running.value.load(std::memory_order_acquire)) running.value.store(false,std::memory_order_release);
        if(getLatestMessage() == simdjson::padded_string{}) ws.next_layer().shutdown(ec);
        return !ec;
    }

    private:
    void executionLoop() {
        ConnectionStatus status;
        if(!(connectionAlive.value.load(std::memory_order_acquire))) status = setupConnection();
        if(status != ConnectionStatus::SUCCESS) {
            status = exponentialBackOff();
            if(status != ConnectionStatus::SUCCESS){
                running.value.store(false,std::memory_order_release);
            }
        }
        connectionAlive.value.store(true,std::memory_order_release);
        Data data;
        while(running.value.load(std::memory_order_acquire)) {
            auto json = getLatestMessage();
            if(json.length() == 0) connectionAlive.value.store(false,std::memory_order_release);
            if(!(connectionAlive.value.load(std::memory_order_acquire))) status = exponentialBackOff();
            if(status == ConnectionStatus::FAIL) {
                running.value.store(false,std::memory_order_release);
                break;
            }
            ThreadSafeParserRAII parser(parserWrapper);

            data = func(json,parser.parser);
            bufferOut.noMoveEnqueue(data);
            controlVariable.value.fetch_add(1,std::memory_order_release);
        }
    }

    public:

    UserDataStream getLatestData() {
        controlVariable.value.fetch_sub(1,std::memory_order_release);
        return bufferOut.dequeue;
    }

    ConnectionStatus reEstablishExecution() {
        closeConnection();
        ConnectionStatus status = setupConnection();

        if(status != ConnectionStatus::SUCCESS) status = exponentialBackOff();
        if(status != ConnectionStatus::SUCCESS) return ConnectionStatus::FAIL;

        if(running.value.load(std::memory_order_acquire)){
            running.value.store(false,std::memory_order_release);
            if(thread.joinable()) thread.join();
        } else {
            if(thread.joinable()) thread.join();
            running.value.store(true,std::memory_order_release);
        }

        thread = std::thread(&WebsocketStreamHolder::executionLoop, this);

        return connectionAlive.value.load(std::memory_order_relaxed)
       ? ConnectionStatus::SUCCESS
       : ConnectionStatus::FAIL;
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
        if(thread.joinable()) thread.join();
    }

};