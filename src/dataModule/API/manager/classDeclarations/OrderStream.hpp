#pragma once
#include "dataModule/API/helperClasses/APIHelpers.hpp"
#include "dataModule/API/helperClasses/placeOrderParameters.hpp"

class APIManager::OrderStream{
    std::string target = "/ws-fapi/v1";
    WebsocketAPIStreamHolder orderStream;
    RequestParameter<std::string> APIKey;
    RequestParameter<std::string> HMACKey;
    std::string privateKey;
    PlaceOrderParameters placeOrder;
    RequestParameter<std::string> methodPlaceOrder;
    RequestParameter<std::string> requestId;
    OrderTracker orderTracker;
    simdjson::ondemand::parser parser;
    OrderContext orderContext;
    // Generates a RequestParameter<std::string> object from inputs
    RequestParameter<std::string> generateParamsForOrder(PositionSide posSide,
            OrderSide orderSide,
            TimeInForce tif,
            OrderType orderType,
            const std::string& pair,
            const std::string& px,
            const std::string& qty) {
        RequestParameter<std::string> out("params");
        
        placeOrder.prepareOrder(posSide,orderSide,tif,orderType,pair,px,qty);
        out.makeRequestFromRequestParameters(
            APIKey,
            placeOrder.positionSide,
            placeOrder.price,
            placeOrder.quantity,
            placeOrder.side,
            placeOrder.symbol,
            placeOrder.timeInForce,
            placeOrder.timestamp,
            placeOrder.type  
        );
        HMACKey.request = hmac_sha256(privateKey,out.request);
        out.addParameterToRequest(HMACKey);
        return out;
    }

    // Generates a random id
    void generateID() {
        requestId.request = boost::uuids::to_string(boost::uuids::random_generator()());
    }
    
    public:
    // Sends an order to binance with all the given parameters and also adds the binance response to the Order class 
    // Returns true if the order has been registered returns false if it has not been registered 
    // If returns false check Order's response to see what happened
    // A new order id is created for each request and written to the given string
    // Will create an Error if exchange refused 
    RequestStatus sendNewOrder(PositionSide posSide,
            OrderSide orderSide,
            TimeInForce tif,
            OrderType orderType,
            const std::string& pair,
            double px,
            double qty,std::string& orderID) {
            try{
                generateID();
                orderID = requestId.request;
                
                // Turn price and quantity to strings since generateParamsForOrder expects std::string
                char bufPx[32];
                char bufQty[32];
                auto [ptrPx, ec1] = std::to_chars(bufPx, bufPx + sizeof(bufPx), px, std::chars_format::fixed, 6);
                auto [ptrQty, ec2] = std::to_chars(bufQty, bufQty + sizeof(bufQty), qty, std::chars_format::fixed, 6);
                std::string pxStr(bufPx, ptrPx);
                std::string qtyStr(bufQty, ptrQty);

                RequestParameter<std::string> params = generateParamsForOrder(posSide,orderSide,tif,orderType,pair,pxStr,qtyStr);
                std::string payload = makeRequestFromRequestParameters(requestId,methodPlaceOrder,params);

                std::cout << payload << std::endl;

                auto json = orderStream.sendRequest(payload);
                std::cout << "response: " << json << std::endl;
                simdjson::ondemand::document doc = parser.iterate(json);

                orderTracker.registerOrderSent(orderID,AstraLib::Time::unixTimestampMS(),px, qty,pair,tif,orderSide,posSide,orderType,doc);
                int64_t status = orderTracker.getOrderStatusint(orderID);
                if( status == 200) {
                    return RequestStatus::SUCCESS;
                } else {
                    if(status / 100 == 4) {
                        return RequestStatus::FAIL;
                    } else {
                        return RequestStatus::UNKNOWN;
                    }
                }
                
            } catch (std::runtime_error& e) {
                std::cout << "Boost couldn't send it: " << e.what() << std::endl;
                return RequestStatus::FAIL;
            }catch(std::exception& e ) {
                std::cout << "Soo error is here: "<< e.what() << std::endl;
                return RequestStatus::FAIL;
            }
            

    }

    OrderStream(APIManager& base_) : orderStream(base_.hostFuturesWebsocketAPI, target),
    privateKey(base_.PrivateKey), APIKey("apiKey",base_.APIKey), HMACKey("signature"),
    methodPlaceOrder("method","order.place"), requestId("id"), orderContext(orderTracker){

    }
};