import socket
from struct import *

HOST_IP = "10.42.0.1"
HOST_PORT = 54523

# message_type, device_id, bpm, confidence, timestamp
TP = pack("!iiiiq", 0 ,23, 112,89,2148192)
EP = pack("!iiiiiiiii", 1, 1004, 1, 4, 2, 3, 1, 2, 3)

sock_test = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


response = sock_test.sendto(EP, (HOST_IP, HOST_PORT))

print(response)

response = sock_test.sendto(TP, (HOST_IP, HOST_PORT))

print(response)