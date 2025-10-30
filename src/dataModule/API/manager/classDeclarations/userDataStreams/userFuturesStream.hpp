#pragma once
#include "userDataStreams.hpp"
#include "userDataStreamClassCreation.hpp"

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
    /*  
        Payload example for listen key 
        {"listenKey":"gwKzdioWPho490C2wogHUt9EF8rfkSxO5EVILWXV7gD0k94n7wP97EEfzA3DURPH"}
    */
    public:
    void getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        auto json = userDataStreamAPI.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
        auto doc = parser.iterate(json);
        doc.find_field("listenKey").get_string(listenKey,true);
    }

    void deleteListenKey() {
        std::string target = "/fapi/v1/listenKey";
        auto json = userDataStreamAPI.sendRequest(target,http::verb::delete_,true,false,listenKeyParameter);
    }

    // non blocking
    UserDataStream getLastMessage() {
        if(userDataStreamWebsocket->controlVariable.value.load(std::memory_order_acquire)) {
            return userDataStreamWebsocket->bufferOut.dequeue();
        } else {
            return UserDataStream();
        }
    }

    UserFuturesStream(APIManager& base_) : userDataStreamAPI(base_.ioc,base_.ctx,base_.hostFutures),
        APIKey(base_.APIKey),
        listenKeyParameter("X-MBX-APIKEY",base_.APIKey), 
        requestId("X-Request-ID", boost::uuids::to_string(boost::uuids::random_generator()())),
        method("method", "userDataStream.start"),
        parameters("parameters")
        {
            getListenKey();
            std::string websocketTarget = "/ws/" + listenKey;
            userDataStreamWebsocket = new WebsocketStreamHolder<UserDataStream,decltype(UserDataStreamClassCreation)>(
                UserDataStreamClassCreation,
                base_.hostFuturesWebsocket,
                websocketTarget,
                base_.websocketParser);
    }

    ~UserFuturesStream() {
        delete userDataStreamWebsocket;
        std::cout << "I atleast destroyed the websocket" << std::endl;
        deleteListenKey();
        std::cout << "Closed it all off" << std::endl;
    }
};
    