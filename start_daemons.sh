#!/bin/bash
shopt -s expand_aliases

echo "killing any existing conf screen" 
screen -S conf -X quit
screen -S conf -dm

#read lines from config
while IFS=$'\n' read -r line
do
    #strip comments
    line=${line%\#*}

    #tokenize on whitespace
    tokens=( $line )
    #echo ${tokens[*]}

    #skip empty (or commented) lines
    if [ -n "${tokens[3]}" ]
    then
        sleep 0.2
        echo "starting ${tokens[0]}"
        screen -S conf -X screen "${tokens[0]}" bash -lc "./configure.exe -b ${tokens[3]} -t ${tokens[2]}"
    fi
done < ltaConfigFile.txt

echo "daemons running, waiting for LTA boot" 
sleep 20
echo "ready for LTA init and dummy image"
