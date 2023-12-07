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

source init/init_skp_lta_v2_smart.sh >> lta.txt

#real LTA rate is about 50 us per samp, so this is about 100x real speed
lta set simUsPerSamp 0.5 >> lta.txt

#we have to disable timestamps for tests, so the file hashes are the same every time
lta setsoft writeTimestamps 0 >> lta.txt

#we have to disable automatic deleting of temp files and .dat files, so we can test readOldDatFiles.exe
lta setsoft keepTemp 1 >> lta.txt
lta setsoft deleteDats 0 >> lta.txt

lta name out_ >> lta.txt
lta tempdir . >> lta.txt
lta sseq sequencers/sequencer.xml >> lta.txt

lta getseq NROW >> lta.txt
lta getseq NCOL >> lta.txt
lta getseq NSAMP >> lta.txt
lta getseq CCDNROW >> lta.txt
lta getseq CCDNCOL >> lta.txt

lta read >> lta.txt

echo "smart sequencer readout done"

lta NCOL 700 >> lta.txt
lta NROW 100 >> lta.txt
lta NSAMP 30 >> lta.txt
lta read >> lta.txt
echo "700x100 smart sequencer readout done"

./readOldDatFiles.exe 30 700 100 out_2_reprocessed out_2_
echo ".dat re-processing done"

lta sseq tests/sequencers/sequencer_dynamic.xml >> lta.txt
lta read >> lta.txt

echo "smart sequencer dynamic readout done"

lta stopsim >> lta.txt
sleep 1
kill %2 # kill the daemon (should already be dead)
kill %1 # kill the simulator (should also be dead)

ls -ltr
shasum out_*.fz out_*.fits #print the shasums to put in the .sha file
shasum -c tests/testsseq.sha #check the .sha file
exit $? #return value is 0 if all checksums matched
