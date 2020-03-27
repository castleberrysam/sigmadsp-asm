#!/usr/bin/env python3

import sys

a = 16807.0
m = 2147483647
q = 127773
r = 2836.0
iseed = 123456.0

def ran():
    global iseed
    seed = iseed
    seed = (a * (seed % q)) - (r * (seed / q))
    if seed < 0.0:
        seed += m
    iseed = seed
    return iseed

with open(sys.argv[1]) as f:
    for line in f:
        line = line.strip()
        if line == 'eol':
            sys.stdout.write('\n')  
        elif line[0] == '`':
            if line[2] == '`':
                sys.stdout.write(' ')
            else:
                sys.stdout.write(line[2])
        else:
            num = int(line) - int((1000.0 * ran() / m))
            if num < 27:
                sys.stdout.write(chr(num+96))
            else:
                sys.stdout.write(chr(num+38))
