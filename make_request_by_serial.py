import serial
import json

port = serial.Serial(
    port="/dev/ttyACM0",
    baudrate=9600
)

def send(v):
    print(json.dumps(v))
    port.write(
        (json.dumps(v) + "\n").encode()
    )
    reply = b""
    while reply == b"":
        reply = port.readline().decode()
    return json.loads(port.read_all())

def send_and_print(v):
    print("REQUEST:")
    print(json.dumps(v, indent=2))
    reply = send(v)
    print("REPLY:")
    print(json.dumps(reply, indent=2))

send_and_print(
    dict(
        method="doit",
        jsonrpc="2.0",
        id=1
    )
)

send_and_print(
    {"jsonrpc": "2.0", "method": "add2", "params": [5, 6], "id": 55}
)