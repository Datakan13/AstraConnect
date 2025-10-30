#pragma once
#include <string>

template<typename T>
class RequestParameter {
    public:
    T requestField = "";
    std::string request = "";
    bool isConstructed = false;

    // When isConstructed is false → produces "requestField": "{}" style JSON (value wrapped in quotes)
    // When isConstructed is true  → produces "requestField": { ... } style JSON (raw JSON or object inserted directly)
    template<typename... Args>
    void makeRequestFromRequestParameters(Args&&... requestParameters) {
        if (((!requestParameters.requestField.empty() && !requestParameters.request.empty()) && ...)) {
            std::string json = "{";
            bool first = true;

            // Expand the pack and concatenate results
            (([&] {
                if (!first) json += ", ";
                else first = false;
                if(requestParameters.isConstructed || requestParameters.requestField == "timestamp") {
                    json += "\"" + requestParameters.requestField + "\": " + requestParameters.request;
                } else {
                    json += "\"" + requestParameters.requestField + "\": \"" + requestParameters.request + "\"";
                }
            }()), ...);
            json += "}";
            isConstructed = true;
            request = json;
        } else {
            request = "";
        }

        
    }

    void addParameterToRequest(RequestParameter<std::string>& param) {
        if (request.empty() || !(request.back() == '}')) return;
        request.pop_back();
        request += std::string(", ") +"\"" + param.requestField + "\": \"" + param.request + "\"}"; 
    }

    RequestParameter(T requestField_,std::string request_) : requestField(requestField_), request(request_) {

    }

    RequestParameter(T requestField_) : requestField(requestField_){

    }
};

template<typename... Args>
std::string makeRequestFromRequestParameters(Args&&... requestParameters) {

        if (((!requestParameters.requestField.empty() && !requestParameters.request.empty()) && ...)) {
            std::string json = "{";
            bool first = true;

            // Expand the pack and concatenate results
            (([&] {
                if (!first) json += ", ";
                else first = false;
                if(requestParameters.isConstructed) {
                    json += "\"" + requestParameters.requestField + "\": " + requestParameters.request;
                } else {
                    json += "\"" + requestParameters.requestField + "\": \"" + requestParameters.request + "\"";
                }
            }()), ...);

            json += "}";
            return json;
        } else {
            return "";
        }

        
}

void addParameterToRequest(std::string& request,RequestParameter<std::string>& param) {
    if (request.empty() || !(request.back() == '}')) return;
    request.pop_back();
    request += std::string(", ") +"\"" + param.requestField + "\": " + param.request + "}"; 
}
