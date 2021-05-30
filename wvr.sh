#!/bin/bash 

if [ "$1" == "site" ]; then 
    cd react && \
    npm run build && \
    cd ../.. && \
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    curl --data-binary "@/Users/temporary/Documents/Arduino/wvr_0.3/build/esp32.esp32.esp32wrover/wvr_0.3.ino.bin" http://192.168.4.1/update --header "content-type:text/plain" ; \
    echo "done"
elif [ "$1" == "code" ]; then 
    echo "compiling"
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    curl --data-binary "@/Users/temporary/Documents/Arduino/wvr_0.3/build/esp32.esp32.esp32wrover/wvr_0.3.ino.bin" http://192.168.4.1/update --header "content-type:text/plain" ; \
    echo "done"
elif [ "$1" == "codenodev" ]; then 
    echo "compiling"
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=1 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    curl --data-binary "@/Users/temporary/Documents/Arduino/wvr_0.3/build/esp32.esp32.esp32wrover/wvr_0.3.ino.bin" http://192.168.4.1/update --header "content-type:text/plain" ; \
    echo "done"
elif [ "$1" == "compile" ]; then 
    echo "compiling"
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation"
elif [ "$1" == "compilenodev" ]; then 
    echo "compiling"
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=0 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation"
elif [ "$1" == "compilewithsite" ]; then 
    echo "compiling website"
    cd react && \
    npm run build && \
    cd ../.. && \
    echo "compiling binary"
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation"
elif [ "$1" == "compilewithsitenodev" ]; then 
    echo "compiling website"
    cd react && \
    npm run build && \
    cd ../.. && \
    echo "compiling binary"
    arduino-cli compile -e --build-property build.code_debug=0 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation"
elif [ "$1" == "flash" ]; then 
    cd .. && \
    echo "uploading" && \
    curl --data-binary "@/Users/temporary/Documents/Arduino/wvr_0.3/build/esp32.esp32.esp32wrover/wvr_0.3.ino.bin" http://192.168.4.1/update --header "content-type:text/plain" ; \
    echo "done"
elif [ "$1" == "ftdi" ]; then 
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    arduino-cli upload -p /dev/cu.usbserial-A50285BI --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done"
elif [ "$1" == "ftdinodev" ]; then 
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=1 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    arduino-cli upload -p /dev/cu.usbserial-A50285BI --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done"
elif [ "$1" == "test" ]; then 
    echo "compiling"
    cd .. && \
    arduino-cli compile -e --build-property build.code_debug=5 --fqbn esp32:esp32:esp32wrover wvr_0.3 && \
    echo "done compilation, uploading" && \
    curl --data-binary "@/Users/temporary/Documents/Arduino/wvr_0.3/build/esp32.esp32.esp32wrover/wvr_0.3.ino.bin" http://192.168.4.1/addfirmware --header "content-type:text/plain" --header "name:test_update" --header "size:100000" ; \
    echo "done"
else
    echo "please use one of flags : site, code, or flash"
fi