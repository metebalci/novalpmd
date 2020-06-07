#!/usr/bin/env python3

import socket
import sys
import datetime
import time

def f(v, x):
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    for y in range(1, 9):
        if v & pow(2, y-1):
            c = 5
        else:
            c = 0
        msg = '0 %d %d %d' % (x, y, c)
        sock.sendto(msg.encode('ascii'), ('127.0.0.1', 8888))

while True:
    now = datetime.datetime.now()
    f(0, 1)
    f(now.year % 100, 2)
    f(now.month, 3)
    f(now.day, 4)
    f(0, 5)
    f(now.hour, 6)
    f(now.minute, 7)
    f(now.second, 8)
    time.sleep(0.2)
