# LTA Daemon

**This is a subtree of the main ltaDaemon repository (which contains SENSEI-specific scripts, and is private). Don't push directly to ltaDaemon_public.**

[![Build Status](https://travis-ci.com/sensei-skipper/ltaDaemon.svg?token=8p9xsuLBwgkozchbZDsW&branch=master)](https://travis-ci.com/sensei-skipper/ltaDaemon)
[![License](https://img.shields.io/badge/license-unknown-lightgrey.svg)](../../)

Daemon for configuring and controlling the LTA readout board.

## Installation

The preferred operating system for running the LTA is Ubuntu 20.04, but the following are also in use: Ubuntu 18.04, RHEL/CentOS/SL 6+7, Debian 10. (macOS should also work but we have no active systems using it.) You can find details on setting up a new Ubuntu 18.04 system [here](https://github.com/sensei-skipper/astroskip/blob/master/doc/LoanerSetup.md). 

The LTA scripts will only work in the `bash` shell. This is not always the default shell; some setups may have `csh` or `tcsh` as default. You can run `ps` in any terminal to see what shell you're running. YOu can use `chsh` to change your default shell to `/bin/bash`, or you can run `bash` in the terminal before running the LTA scripts.

Compiling the LTA daemon requires GCC version 4.7 or newer. Most current Linux distributions (Ubuntu 16.04, Debian 8, CentOS 7) meet this requirement. For CentOS 6 or similar (Scientific Linux 6, RHEL 6), see below.

The LTA Daemon has the following dependencies:
* [cfitsio](https://heasarc.gsfc.nasa.gov/fitsio/)
* [ROOT](https://root.cern.ch/)
* Basic software deveopment tools (these commands should exist: `make`, `pkg-config`, `gcc`)

See below for different installation methods. Recommended methods:
* Get ROOT using the package manager on Ubuntu 16.04, otherwise get the CERN binary distribution.
* Get CFITSIO using the package manager on Ubuntu 18.04, otherwise build it from source.
* Other dependencies should be available in the package manager for your OS.

Once the dependencies are installed, you can download and compile the code from the `source` directory:
```
  git clone https://github.com/sensei-skipper/ltaDaemon.git
  cd ltaDaemon/source
  make clean
  make
```

You can test the daemon with the following commands:
```
./test.sh
./testsseq.sh
```

### Building CFITSIO from source
Download `cfitsio_latest.tar.gz`, then:
```
tar -xzf cfitsio_latest.tar.gz
cd cfitsio-3.47/
./configure
make shared
make install
echo "export FITSIOROOT=${PWD}" >> ~/.bashrc
echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$FITSIOROOT/lib' >> ~/.bashrc
echo 'export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$FITSIOROOT/lib/pkgconfig/' >> ~/.bashrc
```

### Installing ROOT or CFITSIO from a package manager
On Ubuntu 16.04, you can install ROOT with `apt-get install root-system libroot-*-dev` and CFITSIO with `apt-get install libcfitsio-dev`.

On Ubuntu 18.04, you can install CFITSIO with `apt-get install libcfitsio-dev`.

### Installing ROOT with a CERN binary distribution
Select a release from https://root.cern.ch/downloading-root.
Download and unpack the binary distribution matching your OS, and add `source /home/monsoon/Soft/root/bin/thisroot.sh` to your `.bashrc` file.
*Do not get the "Snap" distribution of ROOT. The Snap works well for running ROOT but not for compiling software.*

### GCC 4.7 on CentOS 6
On RHEL-based distributions where the default GCC is pre-4.7, the easiest way to get a newer GCC is to install a developer toolset. The instructions below are for CentOS but similar steps should work for RHEL or Scientific Linux.

As root:
```
yum install centos-release-scl
yum install devtoolset-6
```

Add the following line to your `.bashrc`, which will replace your default environment with the developer toolset at login:
```
source /opt/rh/devtoolset-6/enable
```

### GCC 4.7 on Scientific Linux Fermi 6
As root:
```
yum install http://ftp.scientificlinux.org/linux/slf6x/external_products/devtoolset/yum-conf-devtoolset-1.0-1.el6.noarch.rpm
yum install devtoolset-2
```

Add the following line to your `.bashrc`, which will replace your default environment with the developer toolset at login:
```
source /opt/rh/devtoolset-2/enable
```

### libc on ubuntu
Looks like you need some C-lib headers. This worked on ubuntu 18.04
```
apt install libc6-dev
```

## Configuring UDP buffers
For best performance, you should make your UDP receive buffers larger than the default size.
If you don't do this, data packets from the LTA may sometimes be lost.

On Linux, you can set the buffer sizes with the following commands:
```
sudo sysctl -w net.core.rmem_max=100000000
sudo sysctl -w net.core.rmem_default=100000000
```

and you can make these settings permanent by adding this to `/etc/sysctl.conf`:
```
#expand the receive buffers to reduce LTA packet loss
net.core.rmem_max=100000000
net.core.rmem_default=100000000
```

On OS X, use this command:
```
sudo sysctl -w net.inet.udp.recvspace=3727360
```

## Connecting to the LTA

To configure the network for the LTA on Ubuntu 18.04: 

1. Connect LTA to ethernet port (or to ethernet to USB connector)
2. Configure the ethernet connection to the board
  * Go to "Settings"->"Network"->"Wired" (or PCI Ethernet/USB Ethernet) and click gear icon
  * Select the "IPv4" tab and click the "Manual" radio button
  * Under the "Addresses" section enter Address: 192.168.133.100  Netmask: 255.255.255.0
3. To check that the ethernet configuration worked you should be able to:
`ping 192.168.133.7`

Some more documentation is available on the [Wiki](../../wiki/Network-Configuration).
## Usage

Start a terminal and start the daemon with 
```
./configure.exe
```

Start a new terminal to initialize the LTA configuration
```
source init_skipper.sh
```

Commands can be sent to the LTA through the `lta` command line interface. Several examples can be found below:
```
lta extra # outputs sequencer config variables
lta cols? # outputs the number of columns from the software
lta name images/image # set the output file rootname
lta read # read an image
```
## Sequencer commands
Run the sequencer without taking data:
```
lta runseq
```

Print the sequencer stored in the LTA (legacy sequencer firmware):
```
lta get seq
```

Print the sequencer stored in the LTA (smart sequencer firmware):
```
lta sek recipe print
```

## Master LTA test
You will need xterm installed for the `test_simulators.sh` script to work.

Set up the loopback ports needed for all of the daemons and ltaSimulators (uses sudo):
```
./setup_loopbacks.sh
```

Start the simulators and daemons (when you're done, Ctrl-C this script to kill everything):
```
./test_simulators.sh
```

In a new terminal, send a broadcast command to the daemons, or to a single daemon (identified by the board name in the first column of `ltaConfigFile.txt`):
```
./masterLta.exe -b all get all
./masterLta.exe -b 2 get all
```

Run a readout (each simulator reads the file `test_data.txt` and sends it to its daemon)
```
./test_run.sh
```
