#!/bin/bash

# Run fast cleaning sequence.
# Should be called from top-level dir; assumes aliases from setup_lta.sh.
# Note that the read sequencer will need to be reloaded after cleaning.

seqFastClean=sequencers/sequencer_fast_clean.xml

# Get image dimensions from current sequencer
NROW=$(lta getseq NROW)
NCOL=$(lta getseq NCOL) 
echo "nrow: $NROW, ncol: $NCOL"

doFastClean() {
    # Load the cleaning sequencer and run it
    lta sseq $seqFastClean

    # NROW should be a bit larger than for image (around +50/100)
    # NCOL should be at least 1.5 times larger than image
    # check variables in sequencer file or set here!
    lta NROW $((NROW+50))
    lta NCOL $((NCOL+NCOL))

    lta runseq
}
doFastClean

