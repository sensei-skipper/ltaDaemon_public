#!/bin/bash
vh=-1
vl=-5
hh=-1
hl=-6

#set clock voltages
lta set v1ch $vh
lta set v1cl $vl
lta set v2ch $vh
lta set v2cl $vl
lta set v3ch $vh
lta set v3cl $vl
lta set h1ah $hh
lta set h1al $hl
lta set h1bh $hh
lta set h1bl $hl
lta set h2ah $hh
lta set h2al $hl
lta set h2bh $hh
lta set h2bl $hl
lta set h3ah $hh
lta set h3al $hl
lta set h3bh $hh
lta set h3bl $hl
lta set swah -3
lta set swal -10
lta set swbh -3
lta set swbl -10
lta set rgah 7
lta set rgal -4
lta set rgbh 7
lta set rgbl -4
lta set ogah -4
lta set ogal -8
lta set ogbh -4
lta set ogbl -8
lta set dgh -1
lta set dgl -10
lta set tgh -1
lta set tgl -6

#set bias voltages
lta set vdrain -22
lta set vdd -22 
lta set vr -7
lta set vsub 40

#enable bias switches
lta set vdrain_sw 1
lta set vdd_sw 1
lta set vsub_sw 1
lta set vr_sw 1
lta set p15v_sw 1
lta set m15v_sw 1

