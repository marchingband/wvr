# welcome to wvr

## getting started
* Create a folder on your computer where you will store firmwares for your WVR
* download these 2 firmares, saving them to this folder wvr_safemode.bin and wvr_v1.0.1.bin
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

## setting up for Arduino IDE programming
* install the latest Arduino IDE
* follow instructions online to install the ESP32 stuff
* create a folder called **Libries** in your Arduino sketch folder and unzip WVRduino into that folder
* using the Arduino library manager, install **Async TCP**, **ESP ASYNC WEBSERVER** and **ADAFRUIT NEOPIXEL*
* restart Arduino IDE, create a new sketch, select the **ESP32 WROVER dev board** in the boards menu
* go to file->examples and under **WVR**, open **wvr_basic**
* click **sketch->export compiled binary**, choose a good name, and save the .bin to your WVR firmware folder
* join the **WVR** wifi network, open the WVR UI at http://192.168.5.18/
* click **firware** and then choose a new slot, hit **select binary** for that slot, and choose your .bin file from the file picker
* click **upload**, then click **boot** when upload is complete

Congradulations! You have flashed a custom firmware to your WVR!
