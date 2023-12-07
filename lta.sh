#!/bin/bash
#first append arguments
msg=""                     
for ((i = 2; i <= $#; i++)) ; do
    msg="$msg ${!i}"
done
echo $msg | nc localhost $1
