#pragma once

#include <functional>
#include <unordered_map>
#include <experimental/optional>
#include <ArduinoJson.h>

struct Request {
    const char* method;
    ArduinoJson::JsonVariantConst params;
    ArduinoJson::JsonVariantConst id;

    bool isNotification() const {
        return id.isNull();
    }
};

void convertFromJson(JsonVariantConst src, Request& dst) {
    dst.method = src["method"];
    dst.params = src["params"];
    dst.id = src["id"];
}

bool canConvertFromJson(ArduinoJson::JsonVariantConst src, const Request&) {
    if(not src.is<JsonObjectConst>()) {
        // Serial.println("Not a json object");
        return false;
    }

#ifndef IGNORE_JSONRPC_VERSION
    auto version_elem = src["jsonrpc"];
    if (not (version_elem.is<const char*>() && strcmp(version_elem, "2.0") == 0)) {
        return false;
    }
#endif

    if (not src["method"].is<const char*>()) {
        return false;
    }

    auto id_elem = src["id"];
    if (not (id_elem.is<const char*>() || id_elem.is<int>() || id_elem.isNull())) {
        return false;
    }
    return true;
}



/**
 * @brief A function that can be called remotely
 * 
 * First parameter is the parameters of the function
 * Second parameter is the response object
 * 
 * If the parameters are invalid, the response should contain an error object with the code -32602 and the message "Invalid params"
 * 
 * If the function has a return value, it should be written to the response under the "result" key.
 * 
 */
using RemoteProcedure = std::function<void(ArduinoJson::JsonVariantConst, ArduinoJson::JsonVariant)>;

template<class T>
using optional = std::experimental::optional<T>;


void make_invalid_params_error(ArduinoJson::JsonObject response) {
    auto error = response.createNestedObject("error");
    error["code"] = -32602; // Invalid params
    error["message"] = "Invalid params";
}

void make_invalid_request_error(ArduinoJson::JsonObject response) {
    auto error = response.createNestedObject("error");
    error["code"] = -32600;
    error["message"] = "Invalid request";
}

//When there is not return value
RemoteProcedure makeRemoteProcedure(std::function<bool(ArduinoJson::JsonVariantConst)> call_if_params_match) {
    return [call_if_params_match](ArduinoJson::JsonVariantConst params, ArduinoJson::JsonVariant response) {
        auto params_matched = call_if_params_match(params);
        if(params_matched) {
            response["result"].set(nullptr);
        } else {
            make_invalid_params_error(response);
        }
    };
}

//When there is a return value
template<class T>
RemoteProcedure makeRemoteProcedure(std::function<optional<T>(ArduinoJson::JsonVariantConst)> call_if_params_match) {
    return [call_if_params_match](ArduinoJson::JsonVariantConst params, ArduinoJson::JsonVariant response) {
        auto result = call_if_params_match(params);
        if(result) {
            response["result"] = *result;
        } else {
            make_invalid_params_error(response);
        }
    };
}


struct RemoteProcedureRegistry {
    std::unordered_map<std::string, RemoteProcedure> procedures_by_name;
    
    bool execute_request(const ArduinoJson::JsonDocument& json_request, const ArduinoJson::JsonDocument& json_response) {
        if (json_request.is<ArduinoJson::JsonArray>() && json_request.size() > 0) {
            auto batch_request = json_request.as<ArduinoJson::JsonArrayConst>();
            execute_batch_request(batch_request, json_response.to<ArduinoJson::JsonArray>());
            return json_response.size() > 0;
        } else {
            auto single_request = json_request.as<ArduinoJson::JsonVariantConst>();
            return execute_single_request(single_request, json_response.to<ArduinoJson::JsonObject>());
        }
    }

private:
    bool execute_single_request(ArduinoJson::JsonVariantConst json_request, ArduinoJson::JsonObject response) {      
#ifndef IGNORE_JSONRPC_VERSION
        response["jsonrpc"] = "2.0";
#endif
        
        if(not json_request.is<Request>()) {
            Serial.println("Invalid request");
            serializeJsonPretty(json_request, Serial);
            Serial.println();

            make_invalid_request_error(response);
            response["id"].set(nullptr);
            return true;
        }

        auto request = json_request.as<Request>();
        response["id"] = request.id;

        if (procedures_by_name.find(request.method) == procedures_by_name.end()) {
            auto error = response.createNestedObject("error");
            error["code"] = -32601; // Method not found
            error["message"] = "Method not found";   
            // write all possible names to data field
            auto data = error.createNestedArray("data");
            for(auto it = procedures_by_name.begin(); it != procedures_by_name.end(); it++) {
                data.add(it->first);
            }
        } else {
            auto procedure = procedures_by_name[request.method];
            procedure(request.params, response);
        }
        return not request.isNotification();
    }

    void execute_batch_request(ArduinoJson::JsonArrayConst json_request, ArduinoJson::JsonArray response) {
        for (auto request : json_request) {
            auto response_elem = response.createNestedObject();
            auto should_send_response = execute_single_request(request, response_elem);
            if(not should_send_response) {
                response.remove(response.size() - 1);
            }
        }
    }
};

const char* get_error_message(DeserializationError e) {
    switch(e.code()) {
        case DeserializationError::InvalidInput:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\", \"data\": \"invalid input\"}, \"id\": null}";
        case DeserializationError::EmptyInput:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\", \"data\": \"empty input\"}, \"id\": null}";
        case DeserializationError::IncompleteInput:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32700, \"message\": \"Parse error\", \"data\": \"incomplete input\"}, \"id\": null}";
        case DeserializationError::NoMemory:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32001, \"message\": \"Request too large\"}, \"id\": null}";
        case DeserializationError::TooDeep:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32002, \"message\": \"Request too deep\"}, \"id\": null}";
        default:
            return "{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32603, \"message\": \"Internal error\"}, \"id\": null}";
    }
}