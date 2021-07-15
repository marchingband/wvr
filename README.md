# welcome to wvr

* [getting started](#getting-started)
* [setting up for Arduino IDE programming](#setting-up-for-arduino-ide-programming)
* [using Arduino CLI](#using-arduino-cli)
* [using FTDI](#using-ftdi)


# getting started
* Create a folder on your computer where you will store firmwares for your WVR
* download these 2 firmares, saving them to this new folder

https://github.com/marchingband/wvr_binaries/blob/main/wvr_basic/1.0.1/wvr_basic.ino.bin  https://github.com/marchingband/wvr_binaries/blob/main/wvr_safemode/1.0.1/wvr_safemode.ino.bin


* Apply power to your WVR using a usb cable.
* On a computer, join the wifi network **WVR**, using the password **12345678**
* In a browser, navigate to the address http://192.168.5.18/, and the WVR UI will open
* Click on the blue **WVR** link at the top of the screen
* Click on **select recovery firmware** and choose wvr_safemode.bin in the file upload dialog
* Click on **update recovery firmware** and wait for the upload to complete
* Click on **firmware** menu item at the top of the page
* Click **select binary** for **slot 0**, and select wvr_v1.0.1.bin in the file upload dialog
* Click **upload** then click **boot** when upload is complete

Congradulations! You now have the most up-to-date firmware loaded onto your WVR, and in case something goes wrong, you can boot into a safe-mode firmware by holding D5 to ground when you press reset on the WVR

# setting up for Arduino IDE programming
* install the latest Arduino IDE
* follow instructions online to install the ESP32 stuff
* create a folder called **Libries** in your Arduino sketch folder and unzip WVRduino into that folder
* using the Arduino library manager, install **Async TCP**, **ESP ASYNC WEBSERVER** and **ADAFRUIT NEOPIXEL**
* we also reccomend you intall **ESP Exception Decoder**, in case you want to decode stack traces from the serial monitor, during your Arduino development
* restart Arduino IDE
* go to file->examples and under **WVR**, open **wvr_basic**
* select the **ESP32 WROVER dev board** in the boards menu
* click **sketch->export compiled binary**, the file will be saved in your sketch folder in the **build** folder
* join the **WVR** wifi network, open the WVR UI at http://192.168.5.18/
* click **firware** and then choose a new slot, hit **select binary** for that slot, and choose your .bin file, click the file name and choose a new name for this firmware, so you can keep track of your custom binaries, silly Arduino will always default to the same name.
* click **upload**, then click **boot** when upload is complete

Congradulations! You have flashed a custom firmware to your WVR!

# using Arduino CLI
* install the Arduino CLI
* in a fresh terminal cd into /Arduino/libraries/WVR/examples/wvr_basic
* join the **WVR** wifi network
* type ```./wvr.sh code```, and wait for this sketch to compile and upload to your WVR

# using FTDI
to connect a usb->fdti module to your WVR, connect **D0** to **RX**, **D1** to **TX**, and **GND** to **GND**. Open the sketch examples/wvr_ftdi, where you will see ```wvr->useFTDI = true```. The ESP32 on the WVR needs to be booted into a special FTDI boot mode, to do this, ground **D6** and ground the small copper pad on the top of the WVR labeled "boot" (it's right next to the eMMC), and hit reset. You can release D6 and the boot pad now. The ESP32 is now in FTDI boot mode, and if you have a serial monitor attached the WVR, it should print ```waiting for downlaod``` That's all! Now you can use the **UPLOAD** button in the Arduin IDE, or use ```./wvr.sh ftdi``` from the Arduino CLI to flash, and Arduino Serial Monitor (or any Serial monitor app you like) to get logs from WVR
