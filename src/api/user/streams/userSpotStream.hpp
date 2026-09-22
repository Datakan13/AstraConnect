#pragma once
#include "api/user/userDataStreams.hpp"
#include "core/streams/streams.hpp"
#include "api/common/error/includeErrors.hpp"
// incomplete just needs the same logic from userFuturesStream

class APIManager::User::SpotStream{
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
    FetchError getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        StreamData data = userDataStream.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
        if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
        auto doc = parser.iterate(data.string);
        doc.find_field("listenKey").get_string(listenKey,true);
        return FetchError(APIError::SUCCESS);
    }

    SpotStream(APIManager& base_) : userDataStream(base_.ioc,base_.ctx,base_.hostFutures), APIKey(base_.APIKey),listenKeyParameter("X-MBX-APIKEY",base_.APIKey){
        
    }
};