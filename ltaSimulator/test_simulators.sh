#!/bin/bash
#DAQ PC is 192.168.133.100
#LTA simulators are 192.168.133.10, 11
#delete test data files
rm out*.dat
rm out*.fits
xterm -geometry 60x40+0+0 -hold -e ./ltaSimulator.exe -b 192.168.133.10:22222 &
xterm -geometry 60x40+0+600 -hold -e ./ltaSimulator.exe -b 192.168.133.11:22222 &
xterm -geometry 60x40+800+0 -hold -e ./ltaSimulator.exe -b 192.168.133.12:22222 &
xterm -geometry 60x40+800+600 -hold -e ./ltaSimulator.exe -b 192.168.133.13:22222 &
sleep 1
xterm -geometry 60x40+400+0 -hold -e ./configure.exe -b 192.168.133.10:22222 -l 192.168.133.100 -t 192.168.133.100:8888 &
xterm -geometry 60x40+400+600 -hold -e ./configure.exe -b 192.168.133.11:22222 -l 192.168.133.100 -t 192.168.133.100:8889 &
xterm -geometry 60x40+1200+0 -hold -e ./configure.exe -b 192.168.133.12:22222 -l 192.168.133.100 -t 192.168.133.100:8890 &
xterm -geometry 60x40+1200+600 -hold -e ./configure.exe -b 192.168.133.13:22222 -l 192.168.133.100 -t 192.168.133.100:8891 &

wait
