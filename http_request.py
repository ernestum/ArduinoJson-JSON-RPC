import requests

url = "http://ArduinoJsonRPC.local"

# Send a request to the server
response = requests.post(url, json=dict(
    method="doit",
    jsonrpc="2.0",
    id=1
))

# Print the response
print(response.json())

respone = "xxx"

response = requests.post(url, json={"jsonrpc": "2.0", "method": "add2", "params": [5, 6], "id": 55})
print(response.json())