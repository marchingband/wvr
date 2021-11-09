#!/bin/bash 

VERSION=1.0.5
echo $VERSION

cd ../ && \

cd ../wvr_makers_board && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_makers_board/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_makers_board.ino.bin /Users/temporary/wvr_binaries/wvr_makers_board/$VERSION && \
cd ../wvr_makers_board_usb && \
./wvr.sh compile && \
mkdir /Users/temporary/wvr_binaries/wvr_makers_board_usb/$VERSION && \
cp ./build/esp32.esp32.esp32wrover/wvr_makers_board_usb.ino.bin /Users/temporary/wvr_binaries/wvr_makers_board_usb/$VERSION
