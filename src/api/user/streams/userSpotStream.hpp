#pragma once
#include "api/user/userDataStreams.hpp"
#include "core/streams/streams.hpp"
#include "api/common/error/includeErrors.hpp"
// incomplete just needs the same logic from userFuturesStream

class APIManager::User::SpotStream{
    // Declared first so a missing key throws before anything connects
    std::string APIKey;
    StreamHolder userDataStream;
    std::string listenKey;
    simdjson::ondemand::parser parser;
    RequestParameter<std::string> listenKeyParameter;

        // Wire format: doc/payloads/rest/spot-listen-key.md
    public:
    FetchError getListenKey() {
        std::string target = "/fapi/v1/listenKey";
        StreamData data = userDataStream.sendRequest(target,http::verb::post,true,false,listenKeyParameter);
        if(!data) return FetchError(APIError::BOOST_ERROR,data.ec.message());
        simdjson::ondemand::document doc;
        if(FetchError err = iterateResponse(parser,data.string,doc,data.httpStatus)) return err;
        if(auto ec = doc.find_field("listenKey").get_string(listenKey,true)) {
            return FetchError(APIError::BAD_FIELD,std::string("Failed to parse a field. error: ") + simdjson::error_message(ec));
        }
        return FetchError(APIError::SUCCESS);
    }

    SpotStream(APIManager& base_) : APIKey(base_.requireAPIKey("User::SpotStream")), userDataStream(base_.ioc,base_.ctx,base_.hostFutures), listenKeyParameter("X-MBX-APIKEY",APIKey){
        
    }
};