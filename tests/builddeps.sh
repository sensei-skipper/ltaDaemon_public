#!/bin/bash
# If we're on Linux, ROOT is installed through apt. 
# If we're on OS X, we download a binary (Travis uses 10.13 by default):
if [[ "$TRAVIS_OS_NAME" == "osx" ]]; then
    wget https://root.cern/download/root_v6.18.00.macosx64-10.13-clang100.tar.gz
    tar -xzf root_v6.18.00.macosx64-10.13-clang100.tar.gz
    echo "source ${PWD}/root/bin/thisroot.sh" >> setup.sh
fi
wget http://heasarc.gsfc.nasa.gov/FTP/software/fitsio/c/cfitsio_latest.tar.gz
tar -xzf cfitsio_latest.tar.gz
cd cfitsio-*/
./configure
make shared -j2
make install
# write environment variables to a setup script
echo "export FITSIOROOT=${PWD}" >> ../setup.sh
echo 'export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$FITSIOROOT/lib' >> ../setup.sh
echo 'export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$FITSIOROOT/lib/pkgconfig/' >> ../setup.sh

cd ..
cat setup.sh
cat setup.sh >> ~/.bashrc
