#!/bin/bash
# very crude script to print a sequencer in compiled form
# e.g.
# ./tests/printsseq.sh sequencers/sequencer.xml
#
if [[ "$#" -ne 1 ]]
then
    echo "needs one argument, the sequencer file"
    exit -1
fi

./ltaSimulator.exe -v OFF -b 127.0.0.1:12345 &
sleep 1
./configure.exe -v OFF -b 127.0.0.1:12345 -t 8899 &
sleep 1

#jobs

export daemonport=8899
shopt -s expand_aliases
source setup_lta.sh

lta sseq $1

lta stopsim > /dev/null
sleep 1
kill %2 # kill the daemon (should already be dead)
kill %1 # kill the simulator (should also be dead)

# select the most recent simulator logfile, and grep+cut the commands that were sent to the simulator
cd logs/ltaSimulator.exe
ls -tr|tail -n1|xargs grep "command string"|cut -c108-
