#!/bin/bash
echo "Usage: ./repeat.sh [executable] [proc] [apsParam]"
START=1
END=10
echo "Countdown"
 
for (( c=$START; c<=$END; c++ ))
do
./$1 d 10 d 10 d $2 d $1 $3 
done
 
