#include<functional>
#include<experimental/optional>

#include <Arduino.h>
#include <QNEthernet.h>
#include <ArduinoHttpServer.h>

#include <arduinojson_rpc.h>

using namespace qindesign::network;

template<class T>
using optional = std::experimental::optional<T>;

void doit() {
  Serial.println("Hello");
}

int is_it() {
  return 1;
}

void print_int(int a) {
  Serial.println(a);
}

void add1(int a, int b) {
  auto x = a + b;
}

int add2(int a, int b) {
  return a + b;
}

auto doit_rpc = makeRemoteProcedure([](ArduinoJson::JsonVariantConst params) {
  if(params) {
    return false;
  }
  doit();
  return true;
});

auto is_it_rpc = makeRemoteProcedure<int>([](ArduinoJson::JsonVariantConst params) -> optional<int> {
  if(params) return std::experimental::nullopt;
  return is_it();
});

auto print_int_rpc = makeRemoteProcedure([](ArduinoJson::JsonVariantConst params) -> bool {
  if(not params.is<int>()) return false;
  print_int(params.as<int>());
  return true;
});

auto add1_rpc = makeRemoteProcedure([](ArduinoJson::JsonVariantConst params) -> bool {
  if(not (params.is<ArduinoJson::JsonArrayConst>() && params.size() == 2 && params[0].is<int>() && params[1].is<int>())) return false;
  add1(params[0], params[1]);
  return true;
});

auto add2_rpc = makeRemoteProcedure<int>([](ArduinoJson::JsonVariantConst params) -> optional<int> {
  if(not (params.is<ArduinoJson::JsonArrayConst>() && params.size() == 2 && params[0].is<int>() && params[1].is<int>())) return std::experimental::nullopt;
  return add2(params[0], params[1]);
});


auto server = EthernetServer(80);

auto registry = RemoteProcedureRegistry();



void setup() {
  Serial.begin(9600);
  // while (!Serial) {}

  if(!Ethernet.begin()) {
    // If DHCP fails, use a static IP
    Serial.println("Failed to start ethernet");
  }
  if (!Ethernet.waitForLocalIP(15000)) {
    printf("Failed to get IP address from DHCP\r\n");
    Ethernet.begin(192, 168, 1, 177);
  }

  // Print the IP address
  
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  // Setup MDNS
  MDNS.begin("ArduinoJsonRPC");
  MDNS.addService("_http", "_tcp", 80);

  server.begin();

  registry.procedures_by_name["doit"] = doit_rpc;
  registry.procedures_by_name["is_it"] = is_it_rpc;
  registry.procedures_by_name["print_int"] = print_int_rpc;
  registry.procedures_by_name["add1"] = add1_rpc;
  registry.procedures_by_name["add2"] = add2_rpc;
}

void serial_json_rpc_loop() {

  if(Serial.available() > 2) {
    ArduinoJson::StaticJsonDocument<3000> doc;
    DeserializationError err = deserializeJson(doc, Serial);

    if (err == DeserializationError::Ok) 
    {
      // Execute the request
      ArduinoJson::StaticJsonDocument<3000> response;
      if(registry.execute_request(doc, response)) {
        if(response.overflowed()) {
          Serial.println("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32000, \"message\": \"Response too large\"}, \"id\": null}");
        } else {
          serializeJson(response, Serial);
        }              
      }
    } 
    else 
    {
      Serial.println(get_error_message(err));
  
      // Flush all bytes in the "link" serial port buffer
      while (Serial.available() > 0)
        Serial.read();
    }
  }
}

void http_json_rpc_loop() {
  // Check if a client has connected
  EthernetClient client = server.accept();
  if (client) {
    // an http request ends with a blank line
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        // if you've gotten to the end of the line (received a newline
        // character) and the line is blank, the http request has ended,
        // so you can send a reply
        if (c == '\n' && currentLineIsBlank) {
          // send a standard http response header
          client.writeFully("HTTP/1.1 200 OK\n\r");
          client.writeFully("Content-Type: application/json\n\r");
          client.writeFully("Connection: close\n\r");  // the connection will be closed after completion of the response
          client.writeFully("\n\r");
          // Execute the request
          ArduinoJson::StaticJsonDocument<3000> doc;
          DeserializationError err = deserializeJson(doc, client);

          if (err == DeserializationError::Ok) 
          {
            // Execute the request
            ArduinoJson::StaticJsonDocument<3000> response;
            if(registry.execute_request(doc, response)){
              if(response.overflowed()) {
                client.println("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32000, \"message\": \"Response too large\"}, \"id\": null}");
              } else {
                serializeJson(response, client);
              }              
            }
          } else {
            client.writeFully(get_error_message(err));
          }
          break;
        }
        if (c == '\n') {
          // you're starting a new line
          currentLineIsBlank = true;
        } else if (c != '\r') {
          // you've gotten a character on the current line
          currentLineIsBlank = false;
        }
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
  }
}

void http_library_json_rpc_loop() {
  EthernetClient client = server.accept();
  if(client) {
    if(client.connected()) {
      ArduinoHttpServer::StreamHttpRequest<3000> httpRequest(client);

      if (httpRequest.readRequest())
      {
        Serial.println(httpRequest.getResource().toString());

        auto is_post = httpRequest.getMethod() == ArduinoHttpServer::Method::Post;
        auto is_json = httpRequest.getContentType() == "application/json";
        if(is_post && is_json) {
          ArduinoJson::StaticJsonDocument<3000> doc;
          DeserializationError err = deserializeJson(doc, httpRequest.getBody());

          if (err == DeserializationError::Ok)
          {
            // Execute the request
            ArduinoJson::StaticJsonDocument<3000> response;
            if(registry.execute_request(doc, response)){
              if(response.overflowed()) {
                ArduinoHttpServer::StreamHttpReply httpReply(client, httpRequest.getContentType());
                httpReply.send("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32000, \"message\": \"Response too large\"}, \"id\": null}");
              } else {
                auto responseString = String();
                serializeJson(response, responseString);
                ArduinoHttpServer::StreamHttpReply httpReply(client, httpRequest.getContentType());
                httpReply.send(responseString);
              }              
            }
          } else {
            ArduinoHttpServer::StreamHttpReply httpReply(client, httpRequest.getContentType());
            httpReply.send(get_error_message(err));
          }
        } else {
          ArduinoHttpServer::StreamHttpErrorReply httpReply(client, "application/text");
          httpReply.send("Invalid request. Must be POST with application/json content type.");
        }
      } else {
         // HTTP parsing failed. Client did not provide correct HTTP data or
         // client requested an unsupported feature.
         ArduinoHttpServer::StreamHttpErrorReply httpReply(client, httpRequest.getContentType());

         const char *pErrorStr( httpRequest.getError().cStr() );
         String errorStr(pErrorStr); //! \todo Make HttpReply FixString compatible.

         httpReply.send( errorStr );
      }

    }
    client.stop();
  }
}

void tcp_json_rpc_loop() {
  EthernetClient client = server.accept();
  if(client) {
    if(client.connected()) {

      StaticJsonDocument<3000> json_doc;
      auto err = deserializeJson(json_doc, client);

      if (err == DeserializationError::Ok) {
        // Execute the request
        ArduinoJson::StaticJsonDocument<3000> response;
        if(registry.execute_request(json_doc, response)) {
          if(response.overflowed()) {
            client.writeFully("{\"jsonrpc\": \"2.0\", \"error\": {\"code\": -32000, \"message\": \"Response too large\"}, \"id\": null}");
          } else {
            serializeJson(response, client);
          }              
        }
      } else {
        client.writeFully(get_error_message(err));
      }
    }
    client.stop();
  }
}

void loop() {
  // tcp_json_rpc_loop();
  // http_json_rpc_loop();
  http_library_json_rpc_loop();
  serial_json_rpc_loop();
}