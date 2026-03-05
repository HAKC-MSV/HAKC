#!/bin/sh

EXPLOIT=hakc-demo-exploit

cd /etc/ros2-demo

busybox modprobe rosdemo-leaker
busybox modprobe rosdemo-consumer

mknod /dev/r1 c 509 0
mknod /dev/r2 c 508 0

./$EXPLOIT
