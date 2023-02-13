import socket

HOST_IP = "35.92.239.128"
HOST_PORT = 54523

sock_test = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

response = sock_test.sendto("Socket test".encode(), (HOST_IP, HOST_PORT))

print(response)