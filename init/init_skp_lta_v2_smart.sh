#!/bin/bash
source setup_lta.sh
if [ -n "$voltagescript" ]
then
    source $voltagescript
else
    source voltage_skp_lta_v2.sh
fi
lta nomulti
lta sseq sequencers/sequencer.xml
lta set sinit 30
lta set pinit 0
lta set ssamp 200
lta set psamp 200
lta set packSource 9
lta set cdsout 2 #sig-ped
