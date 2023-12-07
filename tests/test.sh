#!/bin/bash

#delete test data files
rm -f out_*.dat out_*.fz out_*.fits out_*.csv out_*.root
rm -f lta.txt

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

source init/init_skp_lta_v2.sh >> lta.txt

#real LTA rate is about 50 us per samp, so this is about 100x real speed
lta set simUsPerSamp 0.5 >> lta.txt

#we have to disable timestamps for tests, so the file hashes are the same every time
lta setsoft writeTimestamps 0 >> lta.txt

lta name out_ >> lta.txt
lta tempdir . >> lta.txt

lta seq sequencers/legacy/skp_lta_v2.txt >> lta.txt

lta getseq NROW >> lta.txt
lta getseq NCOL >> lta.txt
lta getseq NSAMP >> lta.txt
lta getseq CCDNROW >> lta.txt
lta getseq CCDNCOL >> lta.txt

lta read >> lta.txt

echo "standard readout done"

lta getraw >> lta.txt

echo "raw readout done"

lta NSAMP 30 >> lta.txt
lta read >> lta.txt

echo "NSAMP=30 readout done"

lta NCOL 700 >> lta.txt
lta NROW 100 >> lta.txt
lta read >> lta.txt

echo "700x100 readout done"

#lta NCOL 700 >> lta.txt
#lta NROW 700 >> lta.txt
#lta NSAMP 1500 >> lta.txt
#lta read >> lta.txt
#
#echo "big readout done"

lta set cdsout 1 >> lta.txt
lta seq tests/sequencers/skp_lta_v2_interleaving.txt >> lta.txt

lta calibrate >> lta.txt

echo "interleaving calibration readout done"

lta read >> lta.txt

echo "interleaving readout done"

lta stopsim >> lta.txt
sleep 1
kill %2 # kill the daemon (should already be dead)
kill %1 # kill the simulator (should also be dead)

ls -ltr
shasum out_*.fz out_*.fits out_*.csv #print the shasums to put in the .sha file
shasum -c tests/test.sha #check the .sha file
exit $? #return value is 0 if all checksums matched
