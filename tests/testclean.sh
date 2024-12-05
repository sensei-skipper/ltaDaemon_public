#!/bin/bash

#delete test data files
out=clean-test.txt
rm -f out_*.dat out_*.fz out_*.fits out_*.csv out_*.root
rm -f $out

echo "################## Test Fast Clean ##################" >> $out

#for OS X on Travis, we need to re-source the ROOT environment here
if [ -f "setup.sh" ]; then
    source setup.sh
fi

./ltaSimulator.exe -b 127.0.0.1:12345 &
sleep 1
./configure.exe -b 127.0.0.1:12345 -t 8899 &
sleep 1

jobs

export daemonport=8899
shopt -s expand_aliases
source setup_lta.sh

source init/init_skp_lta_v2_smart.sh >> $out

#real LTA rate is about 50 us per samp, so this is about 100x real speed
lta set simUsPerSamp 0.5 >> $out

#we have to disable timestamps for tests, so the file hashes are the same every time
lta setsoft writeTimestamps 0 >> $out

lta name out_ >> $out
lta tempdir . >> $out

# set the sequencer and take an image
lta sseq sequencers/sequencer.xml >> $out
lta read >> $out
echo "standard readout done"

# run the fast clean
source scripts/fast_clean.sh >> $out
echo "fast clean done"

# need to reload the image sequencer
lta sseq sequencers/sequencer.xml >> $out
lta read >> $out
echo "standard readout done"

lta stopsim >> $out
sleep 1
kill %2 # kill the daemon (should already be dead)
kill %1 # kill the simulator (should also be dead)

exit $? #return value is 0 if all checksums matched
