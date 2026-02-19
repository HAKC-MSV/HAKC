#!/bin/sh

EXPLOIT=hakc-demo-exploit

clang -o $EXPLOIT $EXPLOIT.c

modprobe rosdemo-leaker
modprobe rosdemo-consumer

mknod /dev/r1 c 509 0
mknod /dev/r2 c 508 0

./$EXPLOIT
