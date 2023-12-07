#!/bin/bash
chmod 755 lta.sh
if [ -n "$daemonport" ]
then
    alias lta="$PWD/lta.sh $daemonport"
else
    alias lta="$PWD/lta.sh 8888"
fi

