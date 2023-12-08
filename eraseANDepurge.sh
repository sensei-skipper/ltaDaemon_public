#!/bin/bash

#commented sections will ramp an externally controlled HV supply

#if [ -n "$extVSUB" ]
#then
#    hv_lib.py ramp 0
#fi

#Erase
lta exec ccd_erase

#if [ -n "$extVSUB" ]
#then
#    hv_lib.py ramp $extVSUB
#fi

#Purge
lta exec ccd_epurge

