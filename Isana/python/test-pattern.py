import socket
import sys
import time
import itertools


host = "192.168.2.148"
port = 4242

def getTile(x):
    i = (x + x//360) % 360
    l = i & 0x00ff
    h = 0xff if i > 0xff else 0x00
    if i % 5 == 0:
        return [0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00]
    elif i % 5 == 1:
        return [0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00]
    elif i % 5 == 2:
        return [0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff]
    elif i % 5 == 3:
        return [0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]
    elif i % 5 == 4:
        return [0x01, 0x01, 0x03, 0x03, 0x07, 0x07, 0x0f, 0x0f, 0x1f, 0x1f, 0x3f, 0x3f, 0x7f, 0x7f, 0xff, 0xff]

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
    sock.connect((host, port))
    i = 0
    while True:
        data = bytes(list(itertools.chain.from_iterable([getTile(x) for x in range(i,i+360)])))
        sock.sendall(data)
        i += 360
        print(i)

