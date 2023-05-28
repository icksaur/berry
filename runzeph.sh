killall sxhkd
Xephyr -ac -br -noreset -screen 1024x768 :1 &
sleep 1
DISPLAY=:1 /usr/local/bin/berry