#include<functional>
#include<experimental/optional>

#include <Arduino.h>
#include <NativeEthernet.h>
#include <arduinojson_rpc.h>

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
  
  // Setup Ethernet
  uint8_t mac[6];
  for (uint8_t by = 0; by < 2; by++) mac[by] = (HW_OCOTP_MAC1 >> ((1 - by) * 8)) & 0xFF;
  for (uint8_t by = 0; by < 4; by++) mac[by + 2] = (HW_OCOTP_MAC0 >> ((3 - by) * 8)) & 0xFF;

  if(!Ethernet.begin(mac)) {
    // If DHCP fails, use a static IP
    Ethernet.begin(mac, IPAddress(192, 168, 1, 177));
  }

  // Print the IP address
  
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());

  // Setup MDNS
  MDNS.begin("ArduinoJsonRPC", 1);
  MDNS.addService("_http._tcp", 80);

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
  EthernetClient client = server.available();
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
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: application/json");
          client.println("Connection: close");  // the connection will be closed after completion of the response
          client.println();
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
            client.print(get_error_message(err));
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

void loop() {
  http_json_rpc_loop();
  serial_json_rpc_loop();
}