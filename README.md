# welcome to wvr

* [getting started](#getting-started)
* [setting up for Arduino IDE programming](#setting-up-for-arduino-ide-programming)
* [using Arduino CLI](#using-arduino-cli)
* [using FTDI](#using-ftdi)
* [soldering the USB Backpack](#soldering-the-usb-backpack)


# getting started
* Create a folder on your computer where you will store firmwares for your WVR
* download these 2 firmares, saving them to this new folder
https://github.com/marchingband/wvr_binaries/blob/main/wvr_safemode/1.0.1/wvr_safemode.ino.bin  
and if you have a wvr_basic:
https://github.com/marchingband/wvr_binaries/blob/main/wvr_basic/1.0.1/wvr_basic.ino.bin  
or if you have a USB Backpack as well:  
https://github.com/marchingband/wvr_binaries/blob/main/wvr_usb/1.0.1/wvr_usb.ino.bin  



* Apply power to your WVR using a usb cable.
* On a computer, join the wifi network **WVR**, using the password **12345678**
* In a browser, navigate to the address http://192.168.5.18/, and the WVR UI will open
* Click on the blue **WVR** link at the top of the screen
* Click on **select recovery firmware** and choose wvr_safemode.ino.bin in the file upload dialog
* Click on **update recovery firmware** and wait for the upload to complete
* Click on **firmware** menu item at the top of the page
* Click **select binary** for **slot 0**, and select wvr_basic.ino.bin (or wvr_usb.ino.bin if you have the USB Backpack) in the file upload dialog
* Click **upload** then click **boot** for **slot 0** when upload is complete

Congradulations! You now have the most up-to-date firmware loaded onto your WVR, and in case something goes wrong, you can boot into a safe-mode firmware by holding D5 to ground when you press reset on the WVR

# setting up for Arduino IDE programming
* install the latest Arduino IDE
* follow instructions online to install the ESP32 stuff : https://github.com/espressif/arduino-esp32
* create a folder called **libries** in your Arduino sketch folder and unzip WVRduino into that folder, so it should be Arduino/libraries/WVR/...
* using the Arduino library manager, install **Async TCP**, **ESP ASYNC WEBSERVER** and **ADAFRUIT NEOPIXEL**
* we also reccomend you intall **ESP Exception Decoder**, in case you want to decode stack traces from the serial monitor, during your Arduino development
* restart Arduino IDE
* go to file->examples and under **WVR**, open **wvr_basic**
* select the **ESP32 WROVER dev board** in the boards menu
* click file->save_as and save this as a new sketch (rename it if you like)
* click **sketch->export compiled binary**, the file will be saved in your new sketch's folder in the **build** folder
* join the **WVR** wifi network, open the WVR UI at http://192.168.5.18/
* click **firware** and then choose a new slot, hit **select binary** for that slot, and choose your .bin file from the sketch/build/, in the UI, click the file name and choose a new name for this firmware, so you can keep track of your custom binaries, silly Arduino will always default to the same binary name.
* click **upload**, then click **boot** when upload is complete

Congradulations! You have flashed a custom firmware to your WVR!

# using Arduino CLI
* install the Arduino CLI
* in a fresh terminal use the command ```arduino-cli compile -e --build-property build.code_debug=2 --fqbn esp32:esp32:esp32wrover wvr_basic``` but use your sketch name in place of ```wvr_basic```
* join the **WVR** wifi network
* to flash, you can use curl, the command ```curl --data-binary "@/Users/Username/Documents/Arduino/wvr_basic/build/esp32.esp32.esp32wrover/wvr_basic.ino.bin" http://192.168.5.18/update --header "content-type:text/plain"``` will work, if you change the paths to point at your binary in the build folder within your sketch folder
* in the WVR Arduino library, look at the file ```wvr.sh``` to find some other ideas for things you can do with the arduino-cli, you can modify this bash script to work for you if you like!

# using FTDI
to connect a usb->fdti module to your WVR, connect **D0** to **RX**, **D1** to **TX**, and **GND** to **GND**. Open the sketch examples/wvr_ftdi, where you will see ```wvr->useFTDI = true```. The ESP32 on the WVR needs to be booted into a special FTDI boot mode, to do this, ground **D6** and ground the small copper pad on the top of the WVR labeled "boot" (it's right next to the eMMC), and hit reset. You can release D6 and the boot pad now. The ESP32 is now in FTDI boot mode, and if you have a serial monitor attached the WVR, it should print ```waiting for downlaod``` That's all! Now you can use the **UPLOAD** button in the Arduin IDE, or use ```./wvr.sh ftdi``` from the Arduino CLI to flash, and Arduino Serial Monitor (or any Serial monitor app you like) to get logs from WVR

# soldering the USB Backpack
Both the WVR and the USB Backpack have all the pins labeled, so you should be able to determine the correct orientation. The big USB jack on the backpack is on the same end as the small USB jack on the WVR. The 2 boards go back-to-back.

If you have a WVR Dev Board, there is room to have the USB Backpack on the bottom, but, if you are using the WVR with USB Backpack on a breadboard, then the USB Backpack must be on top. This determines which way the pin headers go in.

Place the pin headers into the WVR Dev Board, or into a breadboard. Place the 2 boards onto the pin headers. There will be JUST enough length that the pin header should just reach through both boards.

Start with one corner (maybe the 5v pin), and touch the soldering iron so you are contacting both the pin header, and the pad, apply lots of pressure, and wait for 3 seconds. Use a thin solder if you have it! Now apply some solder, and the melted solder should wick down into the hole. We are hoping to get lots of solder all the way down, so it joins both boards, and the pin header. Apply more solder little by little, until the hole is full, and a small bulge is left on the top.

Next, re-melt that first solder and with your other hand, press down firmly on the top board, then remove the iron while still pressing down with the other hand. We are hoping to get the boards nice and close and square, and everything in place before moving on.

Now do the same for the opposite pin, on the opposite side (one of the ground pins marked "g"). At this point everything should be secure and look even. Keep going and solder all the pins.

Use a multimeter to check that there are no shorts, and that all the pins are indeed connected to the pads on both boards.

Youre done!
