#!/bin/sh
#Assignment 5 AESD Nicholas Buckley start stop script from lecture linux system init

#Check argument count...
if [ $# -ne 1 ]
then #if it's less than required exit error 1 and print

    echo "$# is not enough arguments."
    echo " please use start or stop arguments"
    exit 1
    
fi

case "$1" in
        start)
            echo "Starting aesdsocket"
            start-stop-daemon -S -n aesdsocket -d /usr/bin/aesdsocket
            ;;
        stop)
            echo "Stopping aesdsocket"
            start-stop-daemon -K -n aesdsocket
            ;;

        *)
            echo "Usage: $0 {start|stop}"
            exit 1

esac

exit 0