#pragma once
#include "dataModule/API/manager/classDeclarations/userDataStreams.hpp"
#include "dataModule/API/manager/lambdaFunctions/userDataStreamClassCreation.hpp"
#include "dataModule/API/APIError.hpp"

// error handling DONE

class APIManager::UserDataStreams::UserFuturesStream{
    StreamHolder userDataStreamAPI;
    WebsocketStreamHolder<UserDataStream,decltype(UserDataStreamClassCreation)>* userDataStreamWebsocket = nullptr;
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
    /*  
        Payload example for listen key 
        {"listenKey":"gwKzdioWPho490C2wogHUt9EF8rfkSxO5EVILWXV7gD0k94n7wP97EEfzA3DURPH"}
    */
    public:
    FetchError getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        simdjson::padded_string json;
        try{
            json = userDataStreamAPI.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
            auto doc = parser.iterate(json);
            doc.find_field("listenKey").get_string(listenKey);
            listenKeyPresent.value.store(true,std::memory_order_release);
            std::cout << listenKey << std::endl;
            return FetchError(APIError::SUCCESS);
        } catch(std::runtime_error& e) {
            return FetchError(APIError::BOOST_ERROR,std::string(e.what()));
        } catch(std::exception& e) {
            return FetchError(APIError::UNKNOWN,std::string(e.what()));
        }
    }

    FetchError deleteListenKey() {
        std::string target = "/fapi/v1/listenKey";
        try {
            auto json = userDataStreamAPI.sendRequest(target,http::verb::delete_,true,false,listenKeyParameter);
            return FetchError(APIError::SUCCESS);
        } catch(std::runtime_error& e) {
            return FetchError(APIError::BOOST_ERROR,std::string(e.what()));
        } catch(std::exception& e) {
            return FetchError(APIError::UNKNOWN,std::string(e.what()));
        }
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

    ConnectionStatus getStatus() {
        while(connecting.value.load(std::memory_order_acquire)) {
            _mm_pause();
        }
        if(!(listenKeyPresent.value.load(std::memory_order_acquire)) || !(connectionAlive.value.load(std::memory_order_acquire)) ) return ConnectionStatus::FAIL;
        return ConnectionStatus::SUCCESS;
    }

    UserFuturesStream(APIManager& base_) : userDataStreamAPI(base_.ioc,base_.ctx,base_.hostFutures),
        APIKey(base_.APIKey),
        listenKeyParameter("X-MBX-APIKEY",base_.APIKey), 
        requestId("X-Request-ID", boost::uuids::to_string(boost::uuids::random_generator()())),
        method("method", "userDataStream.start"),
        parameters("parameters")
        {
            std::atomic_thread_fence(std::memory_order_seq_cst);
            ConnectionStatus status;
            FetchError error = getListenKey();
            if(!error) {
                listenKeyPresent.value.store(true,std::memory_order_release);
            }
            if(!(listenKeyPresent.value.load(std::memory_order_acquire))){
                connecting.value.store(false,std::memory_order_release);
                return;
            }
            std::string websocketTarget = "/ws/" + listenKey;
            userDataStreamWebsocket = new WebsocketStreamHolder<UserDataStream,decltype(UserDataStreamClassCreation)>(
                UserDataStreamClassCreation,
                base_.hostFuturesWebsocket,
                websocketTarget,
                base_.websocketParser);
            status = userDataStreamWebsocket->getStatus();
            if(status != ConnectionStatus::SUCCESS) {
                status = userDataStreamWebsocket->reEstablishExecution();
                if(status != ConnectionStatus::SUCCESS) {
                    connecting.value.store(false,std::memory_order_release);
                    return;
                }
            } else {
                connectionAlive.value.store(true,std::memory_order_release);
            }
            connecting.value.store(false,std::memory_order_release);
    }

    ~UserFuturesStream() {
        delete userDataStreamWebsocket;
        std::cout << "I atleast destroyed the websocket" << std::endl;
        deleteListenKey();
        std::cout << "Closed it all off" << std::endl;
    }
};
    