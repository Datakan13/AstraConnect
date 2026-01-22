#pragma once
#include "dataModule/API/manager/classDeclarations/userDataStreams/userSpotStream.hpp"

// incomplete just needs the same logic from userFuturesStream

class APIManager::UserDataStreams::UserSpotStream{
    StreamHolder userDataStream;
    std::string APIKey;
    std::string listenKey;
    simdjson::ondemand::parser parser;
    RequestParameter<std::string> listenKeyParameter;

        /*
        Payload example for listen key 
        {"listenKey":"CFhC7fBYdyq2PoFnsk9U5lvpCEb5fEyQkjCR0DdvPCCzCZMEt9DCGrKzb"}
    */
    public:
    void getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        auto json = userDataStream.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
        auto doc = parser.iterate(json);
        doc.find_field("listenKey").get_string(listenKey,true);
    }

    UserSpotStream(APIManager& base_) : userDataStream(base_.ioc,base_.ctx,base_.hostFutures), APIKey(base_.APIKey),listenKeyParameter("X-MBX-APIKEY",base_.APIKey){
        
    }
};