#!/bin/bash

#delete test data files
rm -f out_*.dat out_*.fz out_*.fits out_*.csv out_*.root
rm -f lta.txt api-test.txt
unset IFS

echo "################## Test LTA API #####################" >> api-test.txt

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

#load the sequencer
source init/init_skp_lta_v2_smart.sh >> lta.txt

#real LTA rate is about 50 us per samp, so this is about 100x real speed
lta set simUsPerSamp 0.5 >> lta.txt

#we have to disable timestamps for tests, so the file hashes are the same every time
lta setsoft writeTimestamps 0 >> lta.txt

#read an image so that filename variable is set
lta read >> lta.txt

echo -e "\n################ Test Sequencer API #################" >> api-test.txt

echo -e "\n### Old-Style Commands" >> api-test.txt
SEQVARS="CCDNROW:1000 CCDNCOL:500 NROW:100 NCOL:450 NSAMP:10"
for pair in $SEQVARS; do
    IFS=':' read var val <<< "$pair"
    lta $var $val >> lta.txt
    lta getseq $var >> api-test.txt
done

# TODO...
#echo "### New-Style Commands" >> api-test.txt
#SEQVARS="CCDNROW:1001 CCDNCOL:501 NROW:101 NCOL:451 NSAMP:11"
#for pair in $SEQVARS; do
#    IFS=':' read var val <<< "$pair"
#    lta setseq $var $val >> lta.txt
#    ret=$(lta getseq $var)
#    echo "$var: input = $val; output = ${ret##* }" >> api-test.txt
#done

echo -e "\n### Unknown Commands" >> api-test.txt
FAILVARS="ABC123:100"
for pair in $FAILVARS; do
    IFS=':' read var val <<< "$pair"
    lta getseq $var >> api-test.txt
done

echo -e "\n################# Test Software API #################" >> api-test.txt

echo -e "\n### Old-Style Commands" >> api-test.txt
SOFTVARS="writeTimestamps:1 writeTimestamps:0 deleteDats:1 deleteDats:0 keepTemp:0 keepTemp:1"
for pair in ${SOFTVARS}; do
    IFS=':' read var val <<< "$pair"
    lta setsoft $var $val >> lta.txt
    # Filter timestamp
    ret=$(lta getsoft $var)
    echo ${ret##*$'\n'} >> api-test.txt
done

val=./lta_old_
lta multi $val >> lta.txt
lta getsoft multi >> api-test.txt
lta getsoft ltaName >> api-test.txt
lta nomulti >> lta.txt
lta getsoft multi >> api-test.txt

val=./name_old_
lta name $val >> lta.txt
lta name? >> api-test.txt

lta filename? >> api-test.txt

# Trim carriage return
lta tmpdir tmp_old | tr -d $'\r' >> api-test.txt

echo -e "\n### New-Style Commands" >> api-test.txt
# Boolean variables
SOFTVARS="writeTimestamps:true writeTimestamps:false deleteDats:True deleteDats:False keepTemp:False keepTemp:True multi:1 multi:0 multi:TRUE multi:FALSE"
for pair in ${SOFTVARS}; do
    IFS=':' read var val <<< "$pair"
    lta setsoft $var $val >> lta.txt
    
    ret=$(lta getsoft $var)
    echo ${ret##*$'\n'} >> api-test.txt
done
# Make sure timestamps are off
lta setsoft writeTimestamps 0 >> lta.txt

# Non-boolean variables
SOFTVARS="ltaName:lta_new_ name:./name_new_ filename:images/image_lta_1.fz tempdir:tmp_new imageInd:100"
for pair in ${SOFTVARS}; do
    IFS=':' read var val <<< "$pair"
    lta setsoft $var $val >> lta.txt
    lta getsoft $var >> api-test.txt
done

echo -e "\n### Unknown Commands" >> api-test.txt
FAILVARS="ABC123:100"
for pair in $FAILVARS; do
    IFS=':' read var val <<< "$pair"
    lta setsoft $var $val >> api-test.txt
    lta getsoft $var >> api-test.txt
done

echo -e "\n####################### Done ########################" >> api-test.txt

lta stopsim >> lta.txt
sleep 1
kill %2 # kill the daemon (should already be dead)
kill %1 # kill the simulator (should also be dead)

echo -e "\n#################### Check SHAs #####################" >> api-test.txt

shasum api-test.txt #print the shasums to put in the .sha file
shasum -c tests/testapi.sha #check the .sha file
exit $? #return value is 0 if all checksums matched


