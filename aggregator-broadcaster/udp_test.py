import socket
from struct import *

# HOST_IP = "10.42.0.1"
HOST_IP = "35.92.239.128"
HOST_PORT = 54523

# message_type, device_id, bpm, confidence, measure, beat
TP = pack("!iiiiiq", 0 ,23, 62,89,4,4)
EP = pack("!iiiiiiiii", 1, 1004, 1, 4, 2, 3, 1, 2, 3)
REG = pack("!ii", 3, 1005)

sock_test = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)


# response = sock_test.sendto(EP, (HOST_IP, HOST_PORT))
# print(response)

response = sock_test.sendto(REG, (HOST_IP, HOST_PORT))
print(response)

# response = sock_test.sendto(REG, (HOST_IP, HOST_PORT))

