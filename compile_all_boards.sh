#!/bin/bash 

VERSION=1.0.4
echo $VERSION

cd ../ && \
cd ../wvr_basic && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_basic/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_basic.ino.bin /Users/temporary/wvr_binaries/wvr_basic/$VERSION && \
cd ../wvr_usb && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_usb/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_usb.ino.bin /Users/temporary/wvr_binaries/wvr_usb/$VERSION && \
cd ../wvr_makers_board && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_makers_board/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_makers_board.ino.bin /Users/temporary/wvr_binaries/wvr_makers_board/$VERSION && \
cd ../wvr_makers_board_usb && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_makers_board_usb/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_makers_board_usb.ino.bin /Users/temporary/wvr_binaries/wvr_makers_board_usb/$VERSION && \
cd ../wvr_dev_board && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_dev_board/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_dev_board.ino.bin /Users/temporary/wvr_binaries/wvr_dev_board/$VERSION && \
cd ../wvr_dev_board_usb && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_dev_board_usb/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_dev_board_usb.ino.bin /Users/temporary/wvr_binaries/wvr_dev_board_usb/$VERSION && \
cd ../wvr_safemode && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_safemode/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_safemode.ino.bin /Users/temporary/wvr_binaries/wvr_safemode/$VERSION

