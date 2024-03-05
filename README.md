An example of a minimal test implementation of json-rpc using the [ArduinoJson library](https://arduinojson.org/).

The purpose is to find a good balance between library size and the amount of glue code between json-rpc and the user code.

As transports, Serial and HTTP are implemented.

Mostly untested.

Q&A:
- **Why do I have to write so much boilerplate code?** Otherwise we would have to use some template magic, that probably only works in C++17. Also: the thing would only be understandable/modifiable to some C++ wizzards. Also: you would perceive it as magic and would notbe so aware any more that all your funciton calls are squeezed through inefficient JSON.
- **Why use an std::unordered_map for the lookup table?** Because I was lazy. Feel free do use something different. This is not a library, it is an example. Maybe use list of (name, function) tuples sorted buy name and bisect on that list. Maybe sort the list by usage frequency during runtime and do a linear search. Maybe do both approaches. Wow.