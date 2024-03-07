#pragma once

#include <functional>
#include <unordered_map>
#include <experimental/optional>
#include <ArduinoJson.h>

/**
 * @brief A Request according to the JSON-RPC 2.0 specification
 */
struct Request {
    const char* method;
    JsonVariantConst params;
    JsonVariantConst id;

    bool isNotification() const {
        return id.isNull();
    }
};

void convertFromJson(JsonVariantConst src, Request& dst) {
    dst.method = src["method"];
    dst.params = src["params"];
    dst.id = src["id"];
}

bool canConvertFromJson(JsonVariantConst src, const Request&) {
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
using RemoteProcedure = std::function<void(JsonVariantConst, JsonVariant)>;

template<class T>
using optional = std::experimental::optional<T>;


void make_invalid_params_error(JsonObject response) {
    auto error = response["error"].to<JsonObject>();
    error["code"] = -32602; // Invalid params
    error["message"] = "Invalid params";
}

void make_invalid_request_error(JsonObject response) {
    auto error = response["error"].to<JsonObject>();
    error["code"] = -32600;
    error["message"] = "Invalid request";
}

//When there is not return value
RemoteProcedure makeRemoteProcedure(std::function<bool(JsonVariantConst)> call_if_params_match) {
    return [call_if_params_match](JsonVariantConst params, JsonVariant response) {
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
RemoteProcedure makeRemoteProcedure(std::function<optional<T>(JsonVariantConst)> call_if_params_match) {
    return [call_if_params_match](JsonVariantConst params, JsonVariant response) {
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
    
    bool execute_request(const JsonDocument& json_request, JsonDocument& json_response) {
        if (json_request.is<JsonArray>() && json_request.size() > 0) {
            auto batch_request = json_request.as<JsonArrayConst>();
            execute_batch_request(batch_request, json_response.to<JsonArray>());
            return json_response.size() > 0;
        } else {
            auto single_request = json_request.as<JsonVariantConst>();
            return execute_single_request(single_request, json_response.to<JsonObject>());
        }
    }

private:
    bool execute_single_request(JsonVariantConst json_request, JsonObject response) {      
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
            make_invalid_method_error(response);
        } else {
            procedures_by_name[request.method](request.params, response);
        }
        return not request.isNotification();
    }

    void execute_batch_request(JsonArrayConst json_request, JsonArray response) {
        for (auto request : json_request) {
            auto response_elem = response.add<JsonObject>();
            auto should_send_response = execute_single_request(request, response_elem);
            if(not should_send_response) {
                response.remove(response.size() - 1);
            }
        }
    }

    void make_invalid_method_error(JsonObject response) {
        auto error = response["error"].to<JsonObject>();
        error["code"] = -32601; // Method not found
        error["message"] = "Method not found";
        auto data = error["data"].to<JsonArray>();
            for(auto it = procedures_by_name.begin(); it != procedures_by_name.end(); it++) {
                data.add(it->first);
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