#!/bin/bash
vh=5
vl=-2.5
#vl>tl, vh>th, th>vl
th=4.5
tl=-3
#tl>hl, th>hh, hh>tl
hh=4
hl=-3.5
#hl>sl, hh>sh, sh>hl
sh=3.5
sl=-10
#sl<ol, sh>oh
oh=-2.5
ol=-8
rh=7
rl=-4
dh=-1
dl=-10

#set clock voltages
lta set v1ah $vh
lta set v1al $vl
lta set v1bh $vh
lta set v1bl $vl
lta set v2ch $vh
lta set v2cl $vl
lta set v3ah $vh
lta set v3al $vl
lta set v3bh $vh
lta set v3bl $vl
lta set h1ah $hh
lta set h1al $hl
lta set h1bh $hh
lta set h1bl $hl
lta set h2ch $hh
lta set h2cl $hl
lta set h3ah $hh
lta set h3al $hl
lta set h3bh $hh
lta set h3bl $hl
lta set swah $sh
lta set swal $sl
lta set swbh $sh
lta set swbl $sl
lta set rgah $rh
lta set rgal $rl
lta set rgbh $rh
lta set rgbl $rl
lta set ogah $oh
lta set ogal $ol
lta set ogbh $oh
lta set ogbl $ol
lta set dgah $dh
lta set dgal $dl
lta set dgbh $dh
lta set dgbl $dl
lta set tgah $th
lta set tgal $tl
lta set tgbh $th
lta set tgbl $tl

#set bias voltages
lta set vdrain -22	# LTA vdrain range: [-28, -6]
lta set vdd -22		# LTA vdd range: [-28, -6]
lta set vr -7		# LTA vr range: [-15, -6]
lta set vsub 70		# LTA vsub range: [7, 88]

#enable bias switches
lta set vdrain_sw 1
lta set vdd_sw 1
lta set vsub_sw 1
lta set vr_sw 1
lta set p15v_sw 1
lta set m15v_sw 1

