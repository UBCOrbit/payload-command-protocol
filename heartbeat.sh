#! /bin/bash
GPIO=/sys/class/gpio/gpio398
while :
do
    echo 1 > $GPIO/value
    sleep 1
    echo 0 > $GPIO/value
    sleep 1
done
