import socket
from struct import *

HOST_IP = "35.92.239.128"
HOST_PORT = 54523

# message_type, device_id, bpm, confidence, timestamp
TP = pack("!iiiiq", 0 ,23, 112,89,2148192)

sock_test = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

response = sock_test.sendto(TP, (HOST_IP, HOST_PORT))

print(response)