
import socket
import time

s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
r = open("/dev/urandom", "r")

s.sendto("\x02\x01"+"\x00"*10, ('127.0.0.1', 4242))

while True:
    s.sendto(r.read(12), ('127.0.0.1', 4242))
    time.sleep(.05)
