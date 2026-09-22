#pragma once
#include "net/includeBoost.hpp"
#include <AstraLib/AstraLib.hpp>
#include <simdjson/simdjson.h>
#include "core/protocol/requestParameter.hpp"
#include "core/parsing/threadSafeParser.hpp"
#include "core/types/status.hpp"
#include "utils/net/exponentialBackOff.hpp"
#include "api/common/error/includeErrors.hpp"
#include <memory>
#include <utility>

// The first template is the data you want to keep e.g. TradeEvent 
// The second template is your data construction function which must return your data type 
// The second template should also accept simdjson::padded_string
// e.g. [simdjson::padded_string json](){  TradeEvent event = json; return event; }
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
    AstraConnect::Utils::BackoffPolicy backoffPolicy;
    // Signalled whenever `running` is cleared, so a parked backoff wakes at once
    // instead of sleeping out the rest of its interval.
    AstraConnect::Utils::BackoffInterrupt shutdownSignal;
    // Owned by this class. Cleared on every connection attempt, set on failure.
    std::unique_ptr<FetchError> lastError;
    ConnectionStatus setupConnection() {
        connecting.value.store(true,std::memory_order_release);
        lastError.reset();
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
            connectionAlive.value.store(true,std::memory_order_release);
            connecting.value.store(false,std::memory_order_release);
            return ConnectionStatus::SUCCESS;
        } catch(std::exception& e) {
            lastError = std::make_unique<FetchError>(APIError::BOOST_ERROR,std::string(e.what()));
            connecting.value.store(false,std::memory_order_release);
            return ConnectionStatus::FAIL;
        } 
    }

    // Stops early and reports CLOSED when the holder is shutting down.
    ConnectionStatus exponentialBackOff() {
        return AstraConnect::Utils::exponentialBackOff(
            [this]{ return setupConnection(); },
            [this]{ return running.value.load(std::memory_order_acquire); },
            backoffPolicy,
            &shutdownSignal);
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
            lastError = std::make_unique<FetchError>(APIError::BOOST_ERROR,std::string(e.what()));
            return simdjson::padded_string{};
        } 
        
    }


    // Returns the connection status paired with the reason it failed.
    // The FetchError* is owned by this class and is reset on every connection attempt,
    // so copy anything you need out of it before triggering a reconnect.
    // On SUCCESS the pointer is nullptr.
    std::pair<ConnectionStatus,FetchError*> getStatus() {
        while(connecting.value.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        // stopped on purpose, or gave up and shut down -> CLOSED
        if(!(running.value.load(std::memory_order_acquire))) return {ConnectionStatus::CLOSED, lastError.get()};
        // still running, just not connected -> FAIL
        if(!(connectionAlive.value.load(std::memory_order_acquire))) return {ConnectionStatus::FAIL, lastError.get()};
        return {ConnectionStatus::SUCCESS, nullptr};
    }

    bool closeConnection() {
        boost::system::error_code ec;
        if(connectionAlive.value.load(std::memory_order_acquire)) connectionAlive.value.store(false,std::memory_order_release);
        if(running.value.load(std::memory_order_acquire)) running.value.store(false,std::memory_order_release);
        shutdownSignal.signal();
        if(getLatestMessage().length() == 0) ws.next_layer().shutdown(ec);
        return !ec;
    }

    private:
    void executionLoop() {
        ConnectionStatus status = ConnectionStatus::SUCCESS;
        if(!(connectionAlive.value.load(std::memory_order_acquire))) status = setupConnection();
        if(status != ConnectionStatus::SUCCESS) {
            status = exponentialBackOff();
            if(status != ConnectionStatus::SUCCESS){
                running.value.store(false,std::memory_order_release);
                // FAIL or CLOSED: we never got a live connection, so do not advertise one
                return;
            }
        }
        connectionAlive.value.store(true,std::memory_order_release);
        Data data;
        while(running.value.load(std::memory_order_acquire)) {
            auto json = getLatestMessage();
            if(json.length() == 0) connectionAlive.value.store(false,std::memory_order_release);
            if(!(connectionAlive.value.load(std::memory_order_acquire))) status = exponentialBackOff();
            if(status != ConnectionStatus::SUCCESS) {   // FAIL or CLOSED
                running.value.store(false,std::memory_order_release);
                break;
            }
            ThreadSafeParserRAII parser(parserWrapper);

            data = func(json,parser.parser);
            bufferOut.enqueue(data);
            controlVariable.value.fetch_add(1,std::memory_order_release);
        }
    }

    public:
    Data getLatestData() {
        controlVariable.value.fetch_sub(1,std::memory_order_release);
        return bufferOut.dequeue();
    }
    void getLatestData(Data& ref) {
        controlVariable.value.fetch_sub(1,std::memory_order_release);
        ref = bufferOut.dequeue();
    }

    ConnectionStatus reEstablishExecution() {
        closeConnection();
        // The old loop must be finished before we touch the stream again.
        if(thread.joinable()) thread.join();
        // closeConnection() cleared `running`; exponentialBackOff() now honours it,
        // so it has to be re-armed or the retry below reports CLOSED without trying.
        running.value.store(true,std::memory_order_release);
        // signal() is sticky, so clear it or the next backoff returns instantly.
        shutdownSignal.reset();

        ConnectionStatus status = setupConnection();
        if(status != ConnectionStatus::SUCCESS) status = exponentialBackOff();
        if(status != ConnectionStatus::SUCCESS) {
            running.value.store(false,std::memory_order_release);
            return status;   // FAIL or CLOSED, propagated as-is
        }

        thread = std::thread(&WebsocketStreamHolder::executionLoop, this);

        return connectionAlive.value.load(std::memory_order_acquire)
       ? ConnectionStatus::SUCCESS
       : ConnectionStatus::FAIL;
    }

    WebsocketStreamHolder(Func func_, 
    std::string host_, std::string target_,ThreadSafeParser& parser_) : 
    resolver(ioc), ws(ioc,ctx),
    target(target_) ,host(host_), 
    ctx(boost::asio::ssl::context::tls_client), parserWrapper(parser_), 
    func(std::move(func_)), thread(&WebsocketStreamHolder::executionLoop,this) {        

    }

    ~WebsocketStreamHolder() {
        running.value.store(false, std::memory_order_release);
        // Wake a parked backoff, otherwise the join below waits out its interval.
        shutdownSignal.signal();
        if(thread.joinable()) thread.join();
    }

};