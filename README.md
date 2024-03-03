An example of a minimal test implementation of json-rpc using the [ArduinoJson library](https://arduinojson.org/).

The purpose is to find a good balance between library size and the amount of glue code between json-rpc and the user code.

As transports, Serial and HTTP are implemented.

Mostly untested.

Known Issues:
- When I execute `http_request.py`, sometimes I get two times the same reply while I would expect to get two different replies for two different RPCs.