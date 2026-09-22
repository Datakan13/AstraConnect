#pragma once
#include "api/user/userDataStreams.hpp"

#include "api/common/factory/userDataStreamFactory.hpp"

#include "api/common/error/apiError.hpp"
#include "api/common/error/fetchError.hpp"

#include "core/protocol/requestParameter.hpp"

#include "core/streams/streams.hpp"

class APIManager::User::FuturesStream{
    StreamHolder userDataStreamAPI;
    WebsocketStreamHolder<UserDataStream,decltype(parseUserDataStream)>* userDataStreamWebsocket = nullptr;
    std::string APIKey;
    std::string listenKey;
    simdjson::ondemand::parser parser;
    RequestParameter<std::string> listenKeyParameter;
    RequestParameter<std::string> requestId;
    RequestParameter<std::string> method;
    RequestParameter<std::string> parameters;
    AstraLib::Atomic::PaddedAtomic<bool> messagePresent;
    AstraLib::Atomic::PaddedAtomic<bool> listenKeyPresent = false;
    AstraLib::Atomic::PaddedAtomic<bool> connectionAlive = false;
    AstraLib::Atomic::PaddedAtomic<bool> connecting = true; 
    // Owned by this class. Holds the reason the connection was not established,
    // sourced either from the listen key fetch or from the websocket holder.
    std::unique_ptr<FetchError> lastError;
    // FAIL (could not connect) vs CLOSED (holder is shutting down)
    AstraLib::Atomic::PaddedAtomic<ConnectionStatus> lastStatus{ConnectionStatus::FAIL};
    /*  
        Payload example for listen key 
        {"listenKey":"gwKzdioWPho490C2wogHUt9EF8rfkSxO5EVILWXV7gD0k94n7wP97EEfzA3DURPH"}
    */
    public:
    FetchError getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        StreamData data = userDataStreamAPI.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
        if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
        try{
            auto doc = parser.iterate(data.string);
            doc.find_field("listenKey").get_string(listenKey);
            listenKeyPresent.value.store(true,std::memory_order_release);
            return FetchError(APIError::SUCCESS);
        } catch(std::runtime_error& e) {
            std::cout << e.what() << std::endl;
            return FetchError(APIError::BOOST_ERROR,std::string(e.what()));
        } catch(std::exception& e) {
            std::cout << e.what() << std::endl;
            return FetchError(APIError::UNKNOWN,std::string(e.what()));
        }
    }

    FetchError deleteListenKey() {
        std::string target = "/fapi/v1/listenKey";
        StreamData data = userDataStreamAPI.sendRequest(target,http::verb::delete_,true,false,listenKeyParameter);
        if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
        return FetchError(APIError::SUCCESS);
    }

    // non blocking
    RequestStatus getLastMessage(UserDataStream& ref) {
        if(userDataStreamWebsocket->controlVariable.value.load(std::memory_order_acquire) != 0) {
            userDataStreamWebsocket->getLatestData(ref);
            return RequestStatus::SUCCESS;
        } else {
            return RequestStatus::FAIL;
        }
    }

    // Returns the connection status paired with the reason it failed.
    // The FetchError* is owned by this class and is reset on every connection attempt.
    // On SUCCESS the pointer is nullptr.
    std::pair<ConnectionStatus,FetchError*> getStatus() {
        while(connecting.value.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        if(!(listenKeyPresent.value.load(std::memory_order_acquire)) || !(connectionAlive.value.load(std::memory_order_acquire)) ) return {lastStatus.value.load(std::memory_order_acquire), lastError.get()};
        return {ConnectionStatus::SUCCESS, nullptr};
    }

    FuturesStream(APIManager& base_) : userDataStreamAPI(base_.ioc,base_.ctx,base_.hostFutures),
        APIKey(base_.APIKey),
        listenKeyParameter("X-MBX-APIKEY",base_.APIKey), 
        requestId("X-Request-ID", boost::uuids::to_string(boost::uuids::random_generator()())),
        method("method", "userDataStream.start"),
        parameters("parameters")
        {
            lastError.reset();
            FetchError error = getListenKey();
            if(!error) {
                listenKeyPresent.value.store(true,std::memory_order_release);
            } else {
                lastError = std::make_unique<FetchError>(std::move(error));
            }
            if(!(listenKeyPresent.value.load(std::memory_order_acquire))){
                connecting.value.store(false,std::memory_order_release);
                return;
            }
            std::atomic_thread_fence(std::memory_order_seq_cst);
            if(listenKey == "") {
                lastError = std::make_unique<FetchError>(APIError::BAD_FIELD,std::string("Listen key could not be retrived"));
                connecting.value.store(false,std::memory_order_release);
                return;
            }
            std::string websocketTarget = "/ws/" + listenKey;
            userDataStreamWebsocket = new WebsocketStreamHolder<UserDataStream,decltype(parseUserDataStream)>(
                parseUserDataStream,
                base_.hostFuturesWebsocket,
                websocketTarget,
                base_.websocketParser);
            auto [status, err] = userDataStreamWebsocket->getStatus();
            if(status == ConnectionStatus::CLOSED) {
                // holder is shutting down, retrying would only fight the teardown
                lastStatus.value.store(ConnectionStatus::CLOSED,std::memory_order_release);
                if(err) lastError = std::make_unique<FetchError>(err->error,err->getErrorMsg());
                connecting.value.store(false,std::memory_order_release);
                return;
            }
            if(status != ConnectionStatus::SUCCESS) {
                status = userDataStreamWebsocket->reEstablishExecution();
                if(status != ConnectionStatus::SUCCESS) {
                    // adopt the reason from down the stream before a reconnect can reset it
                    auto [retryStatus, retryErr] = userDataStreamWebsocket->getStatus();
                    lastStatus.value.store(retryStatus,std::memory_order_release);
                    FetchError* src = retryErr ? retryErr : err;
                    if(src) lastError = std::make_unique<FetchError>(src->error,src->getErrorMsg());
                    connecting.value.store(false,std::memory_order_release);
                    return;
                }
            } else {
                connectionAlive.value.store(true,std::memory_order_release);
            }
            connecting.value.store(false,std::memory_order_release);
    }

    ~FuturesStream() {
        delete userDataStreamWebsocket;
        std::cout << "I at least destroyed the websocket" << std::endl;
        deleteListenKey();
        std::cout << "Closed it all off" << std::endl;
    }
};
    