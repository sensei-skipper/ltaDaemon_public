#!/bin/bash
source setup_lta.sh
source voltage_skp_lta_v2.sh
lta seq sequencers/legacy/skp_lta_v2.txt
lta set pinit 0
lta set sinit 65
lta set ssamp 189
lta set psamp 189
lta set packSource 9
#lta cols 500

