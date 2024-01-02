#!/bin/bash

make || exit 1

Xephyr -screen 640x480 -screen 640x480 +xinerama :80 &
sleep 1

export DISPLAY=:80

while sleep 1; do berry -c example/autostart ; done
