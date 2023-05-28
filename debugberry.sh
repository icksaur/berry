killall sxhkd
Xephyr -ac -br -noreset -screen 1024x768 :1 &
sleep 1
#DISPLAY=:1 gdbserver :1234 /usr/local/bin/berry
DISPLAY=:1 gdbserver :1234 /usr/local/bin/berry
