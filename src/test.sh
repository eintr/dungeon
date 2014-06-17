#!/bin/sh

N=100

if [ -z $1 ] ; then
	HOST="127.0.0.1"
else
	HOST=$1
fi


while [ $N -ge 0 ]; do
	nc $HOST 9877 < /dev/zero > /dev/null &
	N=$((N-1))
done

wait

exit 0

