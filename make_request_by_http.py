import requests
import time

url = "http://ArduinoJsonRPC.local"


try:
    # Send a request to the server
    response = requests.post(url, json=dict(
        method="doit",
        jsonrpc="2.0",
        id=1
    ))
    print(response.json())
except Exception as e:
    print(e)

# time.sleep(0.1)

# Print the response
# print(response.json())

try:
    response = requests.post(url, json={"jsonrpc": "2.0", "method": "add2", "params": [5, 6], "id": 55})
    print(response.json())
except Exception as e:
    print(e)
# print(response.json())