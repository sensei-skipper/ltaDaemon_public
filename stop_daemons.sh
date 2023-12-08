#!/bin/bash
shopt -s expand_aliases

echo "killing any existing conf screen" 
screen -S conf -X quit
sleep 1
screen -ls
