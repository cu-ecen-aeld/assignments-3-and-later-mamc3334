#!/bin/sh
# aesdsocket-start-stop.sh
# Start/stop script for aesdsocket daemon using start-stop-daemon

# Get the directory of the script and define paths
DIR=$(dirname "$(readlink -f "$0")")
DAEMON=$DIR/aesdsocket

case "$1" in
    start)
        echo "Starting aesdsocket..."
        start-stop-daemon --start --exec $DAEMON -- -d
        echo "aesdsocket started"
        ;;
    stop)
        echo "Stopping aesdsocket..."
        start-stop-daemon --stop --exec $DAEMON --signal TERM
        echo "aesdsocket stopped"
        ;;
    restart)
        echo "Restarting aesdsocket..."
        $0 stop
        sleep 1
        $0 start
        echo "aesdsocket restarted"
        ;;
    *)
        echo "Usage: $0 {start|stop|restart}"
        exit 1
        ;;
esac
exit 0