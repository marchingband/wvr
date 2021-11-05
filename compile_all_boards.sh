#!/bin/bash 

cd ../ && \
cd ../wvr_basic && \
./wvr.sh compile && \
cd ../wvr_usb && \
./wvr.sh compile && \
cd ../wvr_makers_board && \
./wvr.sh compile && \
cd ../wvr_makers_board_usb && \
./wvr.sh compile && \
cd ../wvr_dev_board && \
./wvr.sh compile && \
cd ../wvr_dev_board_usb && \
./wvr.sh compile && \
cd ../wvr_safemode && \
./wvr.sh compile