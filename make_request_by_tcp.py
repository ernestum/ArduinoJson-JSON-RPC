import socket
import json

HOST = 'ArduinoJsonRPC.local'
PORT = 80


def send_json(request):
    try:
        # Create a TCP socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect to the server
        sock.connect((HOST, PORT))
        
        request_data = json.dumps(request).encode('utf-8')
        print("sending:", request_data)
        sock.sendall(request_data)
        
        # Receive the response
        
        response_data = b""
        while True:
            chunk = sock.recv(1024)
            if not chunk:
                break
            response_data += chunk
        # response = json.loads(response_data.decode('utf-8'))
        print("received:", response_data)
        
        
        # Close the socket
        sock.close()
    except Exception as e:
        print(e)
        return
    # return response

send_json(dict(
        method="doit",
        jsonrpc="2.0",
        id=1
    )
)

send_json(
    {
        "method": "add2",
        "jsonrpc": "2.0",
        "params": [5, 6],
        "id": 55
    }
)