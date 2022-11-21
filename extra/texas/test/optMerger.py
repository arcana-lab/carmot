#!/bin/python
import sys
import os

offMap = {}
printOut = []

f = open(sys.argv[1], "r")
content = f.readlines()

first = 0

firstLine = ""

for line in content:
    if first == 0:
        first = 1
        firstLine = str(int(line.split()[0],16))
        continue
    key = line.split()[0]
    off = int(line.split()[1], 16)
    offMap[key] = off

f = open(sys.argv[2], "r")
content = f.readlines()
firstLine = firstLine + " " + str(len(content))
printOut.append(firstLine)
for line in content:
    printOut.append(offMap[line.split()[0]])

f = open(sys.argv[3], "w")
for entry in printOut:
    f.write(str(entry) + "\n")
