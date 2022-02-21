# welcome to wvr

Join us on the WVR Forum : https://groups.google.com/g/wvr-audio  
Find the Pinouts and download the schematics for WVR :  https://github.com/marchingband/wvr_hardware  
Binaries for all the WVR boards are here : https://github.com/marchingband/wvr_binaries  
Code for the Web UI is here : https://github.com/marchingband/wvr_ui  
Code for the WVR USB Backpack is here : https://github.com/marchingband/wvr_usb_backpack  
If you have Thames : WVR in a Pedal, go here : https://github.com/marchingband/wvr_thames

* [getting started](#getting-started)
* [powering wvr](#powering-wvr)
* [updating firmware](#updating-firmware)
* [playing sounds](#playing-sounds)
* sound settings
* * [understanding priority](#understanding-priority)
* * [understanding exclusive group](#understanding-exclusive-group)
* * [understanding response curve](#understanding-response-curve)
* * [understanding retrigger mode](#understanding-retrigger-mode)
* * [understanding note off](#understanding-note-off)
* [using racks](#using-racks)
* [bulk uploading files](#bulk-uploading-files)
* [bulk uploading racks](#bulk-uploading-racks)
* [pitch interpolation](#pitch-interpolation)
* [using fx](#using-fx)
* [global settings](#global-settings)
* [firmware manager](#firmware-manager)
* [setting up for Arduino IDE programming](#setting-up-for-arduino-ide-programming)
* [using Arduino CLI](#using-arduino-cli)
* [using FTDI](#using-ftdi)
* [soldering the USB Backpack](#soldering-the-usb-backpack)


# getting started

* On a computer, join the wifi network **WVR**, using the password **12345678**
* Open Google Chrome (or another browser which [impliments the Web Audio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)), navigate to the address http://192.168.5.18/, and the WVR UI will open

# powering wvr

* **wvr basic** : plug in usb, or apply 5v and ground, or 3.3v and ground, to the power pins
* **wvr makers board** : plug in usb (will not power amp), use a center-negative 9v PSU (Boss style) or apply somewhere between 6v and 9v (and ground) to the VIN pins
* **wvr dev board** : plug in usb, use a center-negative 9v PSU (Boss style) or apply somewhere between 6v and 9v and ground to the VIN pins
* **thames** : use a center-negative 9v PSU (Boss style)
* **usb host backpack** : The backpack is powered by the WVR, so if WVR is on, the backpack is also on. When updating firmware on the backpack, plug in the usb micro port on the backpack **instead of** powering the WVR. It has its 5v line connected to the WVR 5v pin. WVR takes this 5v line, passes it to the LDO, and passes 3.3v back to power the backpack. The 5v pin on the USB host port is also connected to the WVR 5v pin, so it can power whatever is plugged into it, adding some capacitance to meet the USB spec.

# updating firmware

* Create a folder on your computer where you will store firmwares for your WVR
* download your firmwares, saving them to this new folder
* navigate to :
https://github.com/marchingband/wvr_binaries
and find the folder for your board, download the .ino.bin file, which will be a link like:
https://github.com/marchingband/wvr_binaries/blob/main/wvr_basic/1.0.10/wvr_basic.ino.bin
Choose the newest binary (currently v1.0.10)
* Apply power to your WVR.
* On a computer, join the wifi network **WVR**, using the password **12345678**
* Open Google Chrome (or another browser which [impliments the Web Audio API](https://developer.mozilla.org/en-US/docs/Web/API/Web_Audio_API)), navigate to the address http://192.168.5.18/, and the WVR UI will open
* Click on **firmware** menu item at the top of the page
* Click **select binary** for **slot 0**, and in the file upload dialog select the firmware that you downloaded earlier
* To give the ninary a custom name, click the name of the binary at the left.
* Click **upload** for **slot 0**, and wait for the upload to complete
* Click **boot** for **slot 0**, and wait for the boot to complete. 
* Reset the WVR, rejoin the WVR wifi network, then reload the webpage.
* if you have a WVR with USB Host Backpack, you can follow these instructions to update the firmware on that : https://github.com/marchingband/wvr_usb_backpack

Congradulations! You now have the most up-to-date firmware loaded onto your WVR

# playing sounds

* in the WVR UI, click on a note ... maybe ... E2.
* now click **select file**
* a window will open, choose any sound file from your computer
* you can click **audition** and it will play on your computer speakers
* now click **pins** at the top. this is the pin configuration page
* click on **D2**
* on the left, you will see the options for that pin.
* set the **note** to **E2 (40)**
* set the **edge** to **falling** (this means that it will trigger notes when the pin goes from high to low, when you ground it. All the pins which have pullups have them turned on by default, so grounding the pins is like pressing a key on a piano. Setting the edge is important, the default setting is **none** so no sounds will be triggered by this pin untill it is changed.
* set the debounce time : If you are manually grounding the pin with a wire use 120ms, if you have a mechanical button 60ms is good, if you are using an external microcontroller, you could use 0ms. If you get multiple triggers, and multiple sounds playing when you ground a pin, raise the debounce time :) Google "mcu debounce" if you want to learn more about this.
* you will notice that some pins have a touch/digital option. This makes the pins touch sensitive. The same logic applies with the edge : touching the pin with a finger causes a falling edge, and 60ms is a great starting point for a debounce time. The WVR calibrates itself for capacitive touch everytime it is reset, so, it is important to have your hands away from the WVR when you reset it, so it can do an accurate calibration. If it seems off, just reset the WVR and keep your hands clear for a few seconds.
* if you click on **action** you can see all the possible ways to use these pin events. It can trigger a sound, but it can also do many other things.
* **velocity** is the Midi term for volume, so, set this to the playback volume that you would like to result from events on this pin.
* when you have everything set up the way you want, click on **sync**. this will upload your sounds to the WVR, and it will also send the configuration information. Nothing will actually change on the WVR untill you hit **sync**
* wait for the sync to complete. You will see your pin configs have updated, and your sounds are displayed in the sounds menu. Now when you press **audition** the sound will not play in the browser, it will play on the WVR.
* connect headphones or some line out to the WVR
* using a wire (or a female jumper cable) connect the **GND** pin on the WVR to pin **D2**, and your sound should play.
* Changes to the Pin Configuration will not take effect until you reset the WVR, so get in the habit of reseting your WVR as you make changes, after perforing a SYNC. WVR only takes a few seconds to boot and present WIFI, so it is not a real bottle neck in your workflow, your computer will not even notice that the network has been reset.

# understanding priority
WVR can playback up to 18 stereo sounds at once. It mixes all the sounds into a stereo output. If you play very fast, or play dense chords, or have very long sounds, it's possible to ask the WVR to play back more then 18 sounds. When this happens, WVR runs an algorithm to figure out what to do. It will try to find an old, or an unimportant sound, stop playing that sound, and play the newly triggered sound instead. You can help it make this decision by giving some sounds higher **priority**. A lower priority sound will never stop a higher priority sound, only equal or lower priority. In the case where all 18 voices are busy playing high priority sounds, and a lower priority sound is triggered, WVR will not play the sound.

# understanding exclusive group
**Exclusive Group** allows you to tell WVR that only one member out of a group of notes can be playing at one time. If you want your open hihat sample to stop when a closed hihat sample starts, or if you want one bass note to stop when another starts, you can achieve this by setting the **exclusive group** value of all the notes in that group to the same number ie. add them all to the same exclusive group. These groups work across voices and channels. When a note-on is received on a memeber of an exclusive group, any other note in that group that is playing will be immidiately stopped. As with all the note settings, you can select multiple notes using shift-click or command-click, and change the exclusive group for all selected notes.

# understanding response curve
Every MIDI note has a velocity (or volume) attached to it. This is a value from 0 to 127. Imagine a graph with these 127 velocities on the y axis, and the playback volume that each actually triggers on the x axis. If this is a streight line, we have a **linear** response curve. Many people find other curves to be more human, or more musical. The default response curve for WVR is the **Square Root** algorithm, but you can also choose **linear** or **logarithmic**.

# understanding retrigger mode
If a sound is triggered a second time, at a time when it is already in playback, this is a "retrigger". WVR can resond to this event in a number of ways. It can respond by stopping the sound (note-off), restarting the sound from the beginning (restart), ignoring the second trigger (ignore) or by starting a new playback of the same sound without stopping the first (retrigger).

# understanding note off
When a key is lifted on a piano, and likewise, when a pin on the WVR moves from a LOW back to a HIGH state, a "note-off" event is triggered. You can opt to ignore these events by choosing the "ignore" setting, or you can choose to observe them by selecting the "halt" setting, in which case the sound will fade out very fast, and stop, when the note-off event for that sound occurs.

# using racks
The term "rack" is our word for multi-sample functionality. With a traditional midi instrument, the sound engine responds to velocity data by modulating the **volume** of the sounds it produces. In a multi-sampled instrument, instead, the engine responds to velocity by playing a different sample, presumably a sample that reflects a lighter or heavier touch. In the case of a drum sample, the engine should have samples of the same drum being struck with various amounts of force, for example. To create a rack for a given note, click **create rack** in the UI. Next it's a good idea to name this rack, so hit **name rack** and enter a name. You should have all your samples prepared in advance, so next, hit **number of layers** and let WVR know how many samples you have for this rack. Be careful because changing this number in the future clears the data. The UI will automatically set "break points" evenly for each sample, but you can click any layer, and modify the break point if you like. The formula "< 50" you see in the UI indicates that this is the sample that will be triggered if the velocity is below 50, but above the break point for the previous layer. In other words this number sets the upper bound for this layer. You can still use FX while in Rack mode, but the same FX will be applied to all the layers.

# bulk uploading files
You can use **shift-click** and **command-click** to select regions of notes (and regions of racks). If you click **select files** while a range is selected, then the file dialog that comes up will allow you to select multiple files. The files you select will be sorted alphanumerically, and placed in order into the notes you have selected.

# bulk uploading racks 
If you **shift-click** on **select files** while a region of notes are selected, a special file upload dialog will appear, which allows you to select a **directory**. This function allows you to bulk upload some racks. The structure of the folder that you select must be organized in a very specific way. It must be a folder of sub-folders. Each subfolder represents a rack, and contains all the files for that rack. It will use the name of the subfolder for the name of the rack, and it will create as many slots as there are files. It will then sort all the folders, and all the files in all the folders alphanumerically, and place them into racks in the notes that you have selected.

# pitch interpolation
If you select a range of notes, and then **shift-command-click** a note, the note will turn red, and it becomes the **pitch interpolation target**. Click **select file** and select one file from the dialog. The selected file will be pitch interpolated across the range you have selected. The **pitch interpolation target** will maintain its pitch, and all the other notes you selected will be pitched up or down in relation to it. You can look at the **FX** tab to see how the algorithm has decided to pitch your notes.

# using fx
WVR uses the web browsers built-in audio engine to do a lot of work preparing samples before it sends audio data to the WVR. It converts any sample that you choose to a standard audio format of 44.1k 16bit stereo PCM. It also adds FX. Currently the WVR UI impliments Distortion, Reverb, Pitch Shift, Panning and Volume. These FX are rendered before being sent to WVR, so the processor on WVR doesn't need to do any extra signal processing. This does mean that the reverb and it's tail are hard coded into the sample. If you have a sample with reverb, you must set up the note to ignore note-off events, otherwise there will not be a reverb tail if the note off event occurs before the sound is finished playing. You cannot return to the UI later and modify the FX settings, after you have SYNC'd the data to WVR. To change the FX you would need to select the original sound file again from your computer, apply the FX in a new way, and hit SYNC again. Click **audition** to hear how the FX sound. Depending on your computer and web browser, this rendering process is sometimes buggy, it may freeze for a moment (or a few seconds) before playback begins.

# global settings
In the UI, click the blue **WVR** button top and center of the screen. This is the **Global Settings** menu.
* **global volume** sets the overall volume of the WVR output. A value of 127 mean full volume, and 0 means mute.
* WVR has a **recovery mode** functionality that you can set up here. If you hold the **recovery pin** to ground and push reset on WVR, code will stop executing, and a special **recovery mode** UI will be served. This means that you can experiment with firmwares, and always have a fallback incase there is an error in your code that may prevent you from accessing the UI. Just make sure that wvr.begin() is called before your custom code.
* If you open the javascript console in your web browser, you will see some logs coming from the WVR. You can set the **log verbosity**. If you don't need logs, leave it set to **none**. Verbose logs can cause little glitches in the audio engine.
* **wifi on at boot** determines weather your WVR will turn on its wifi antenna at boot. Setting this to **false** is risky. Make sure that you have a pin action set to turn it on, and have tested that pin, because if you don't, its possible you will be unable to access the UI to change this setting back! The solution could be to boot into recovery mode, of course, should that occur.
* you can also change the WIFI network name and password here.
* **wifi power** allows you to change how strong the wifi signal is. Higher numbers mean more range, which also draws more power, creating more heat. At **8** the range should fill a room.
* WVR has memory dedicated to store up to 128 racks. **rack slots remaining** lets you keep track of how many are free.
* Backing up and restoring your eMMC (onboard memory on the WVR) is meant for cloning your WVR, in the case where you have a product that you want to quickly clone, and not have to set up all the configuration and sounds every time. **backup eMMC** will upload a binary file to your computer. On the receiving device, open this same menu and **select eMMC recovery file** then **restore eMMC**. This process may take a long time, depending on how many sounds you have in memory. Note this process will clone everything *except* the firmware running on your WVR, so make sure you have the same (or compatible) firmwares running on both devices. All the note configurations, all the sounds, the backup firmware, your stored firmwares, the global settings, and everything else will be cloned to the new WVR!
* If you want to reset the memory on your WVR, you can click **reset eMMC**. All sounds, configuration, and firmwares will be deleted, but the firmware currently loaded will remain in flash, and will continue to run.
* You can upload a firmware directly to the ESP32 flash, and boot it, without using the **firmware** menu, by clicking **select firmware**, choosing a binary, and then clicking **force uplad**. This binary will not be stored in eMMC memory, so it will not appear in the **firmware** menu. It will stay in flash, and continue to run untill a new firmware is force-uploaded, or untill another firmware is selected from the **firmware** tab.

# firmware manager
WVR can store up to 10 different firmwares, and the UI allows you to boot from any of them.
Click **select binary** for the slot you want to use, and find the compiled binary on your computer. Click on the filename (at the left) and rename your firmware so you know what it is. Click **upload** and wait for it to complete. After refreshing the browser, click **boot** and wait for the firmware to boot. It's a good idea to reset the WVR after this process, then refresh the browser. You are now in your new firmware! Note that some firmwares are not compatible. If the partition scheme on the eMMC is different between firmwares, the WVR will notice, and will have to reformat the eMMC, this means you will loose all stored configuration and sounds, and all your firmwares, etc. Releases of WVR firmware are versioned to make this clear. 1.0.1 -> 1.0.2 or 1.1.1 is safe, but 1.0.1 -> 2.0.1 would not be safe. We will aim to only VERY RARELY make a major version bump, but there may be times when it is necisary. Other developers who may release their own WVR firmwares in the future SHOULD CERTAINLY follow this tradition, and indicate which version of WVR firmware they are targeting :) <3

# setting up for Arduino IDE programming
* install the latest Arduino IDE
* follow instructions online to install the ESP32 stuff : https://github.com/espressif/arduino-esp32
* In the Arduino Boards manager, when you install the ESP32 board, please select version 2.1, which is **not** the default version.
* donwload the WVR Arduino library here https://github.com/marchingband/wvr/releases/tag/v1.0.9
* create a folder called **libries** in your Arduino sketch folder and unzip the **WVR Arduno library** into that folder, so it should be Arduino/libraries/WVR/...
* using the Arduino library manager, install **ADAFRUIT NEOPIXEL**
* download https://github.com/me-no-dev/ESPAsyncWebServer and https://github.com/me-no-dev/AsyncTCP (click **CODE** -> **download zip**) and then unzip them into the **libraries** folder as well.
* we also reccomend you intall **ESP Exception Decoder**, in case you want to decode stack traces from the serial monitor, during your Arduino development
* restart Arduino IDE
* go to file->examples, and under **WVR**, open **wvr_basic**
* select the **ESP32 Wrover Module** in the boards menu
* click file->save_as and save this as a new sketch (rename it if you like)
* click **sketch->export compiled binary**, the file will be saved in your new sketch's folder in the newly created **build** folder
* join the **WVR** wifi network, open the WVR UI at http://192.168.5.18/
* click **firware** and then choose a new slot, hit **select binary** for that slot, and choose your .bin file from the your_sketch/build folder, in the UI, click the file name and choose a new name for this firmware, so you can keep track of your custom binaries, silly Arduino will always default to the same binary name.
* click **upload**, then click **boot** when upload is complete

Congradulations! You have flashed a custom firmware to your WVR!

# using Arduino CLI
* install the Arduino CLI
* in a fresh terminal use the command ```arduino-cli compile -e --build-property build.code_debug=2 --fqbn esp32:esp32:esp32wrover wvr_basic``` but use your sketch name in place of ```wvr_basic```
* join the **WVR** wifi network
* to flash, you can use curl, the command ```curl --data-binary "@/Users/Username/Documents/Arduino/wvr_basic/build/esp32.esp32.esp32wrover/wvr_basic.ino.bin" http://192.168.5.18/update --header "content-type:text/plain"``` will work, if you change the paths to point at your binary in the build folder within your sketch folder
* in the WVR Arduino library, look at the file ```wvr.sh``` to find some other ideas for things you can do with the arduino-cli, you can modify this bash script to work for you if you like!

# using FTDI
to connect a usb->fdti module to your WVR, connect **D0** to **RX**, **D1** to **TX**, and **GND** to **GND**. Open the sketch examples/wvr_ftdi, where you will see ```wvr->useFTDI = true```. The ESP32 on the WVR needs to be booted into a special FTDI boot mode, to do this, ground **D6** and ground the small copper pad on the top of the WVR labeled "boot" (it's right next to the eMMC), and hit reset. You can release D6 and the boot pad now. The ESP32 is now in FTDI boot mode, and if you have a serial monitor attached the WVR, it should print ```waiting for downlaod```. Now you can use the **UPLOAD** button in the Arduin IDE, at the end of flashing it will print "hard reseting", now restart the WVR. If you open the Arduino Serial Console, you will see some logs form the WVR boot process. With FTDI, you can also use ```./wvr.sh ftdi``` to flash, and Arduino Serial Monitor (or any Serial monitor app you like) to get logs from WVR

# soldering the USB Backpack
Both the WVR and the USB Backpack have all the pins labeled, so you should be able to determine the correct orientation. The big USB jack on the backpack is on the same end as the small USB jack on the WVR. The 2 boards go back-to-back.

If you have a WVR Dev Board, there is room to have the USB Backpack on the bottom, but, if you are using the WVR with USB Backpack on a breadboard, then the USB Backpack must be on top. This determines which way the pin headers go in.

Place the pin headers into the WVR Dev Board, or into a breadboard. Place the 2 boards onto the pin headers. There will be JUST enough length that the pin header should almost reach through both boards.

If you are using a breadboard, this can be tricky, because the WVR is a little too tall, and the pins don't quite go all the way in. You may need to be a bit creative here. You could use female pin headers to get some extra height if you have them. You may want to put some pieces of wire, or a toothpick, under the plastic parts of the pin headers, so everyting fits really snug and flush. You want to be able to apply a good amount of pressure with the soldering iron, and not have things move around. Soldering is so fun, especially when there are challenges! You've got this!

here is an image (this shows the USB Backpack on top) : https://imgur.com/a/3H6OtFo

Start with one corner (maybe the 5v pin), and touch the soldering iron so you are contacting both the pin header, and the pad, apply lots of pressure, and wait for 3 seconds. Use a thin solder if you have it! Now apply some solder, and the melted solder should wick down into the hole. We are hoping to get lots of solder all the way down, so it joins both boards, and the pin header. Apply more solder little by little, until the hole is full, and a small bulge is left on the top.

Next, re-melt that first solder and with your other hand, press down firmly on the top board, then remove the iron while still pressing down with the other hand. We are hoping to get the boards nice and close and square, and everything in place before moving on.

Now do the same for the opposite pin, on the opposite side (one of the ground pins marked "g"). At this point everything should be secure and look even. Keep going and solder all the pins.

here is an image (this one has the WVR on top) : https://imgur.com/a/CyD6zfh

Use a multimeter to check that there are no shorts between adjacent pins, and that all the pins are indeed connected to the pads on both boards.
If the USB Backpack is on top, place one probe of your multimeter on the castellated edge of the WVR, and check for continuity with the pin header. Here is a photo of that procedure : https://imgur.com/a/iM4eVyv
It is very helpful to have flux, in particular liquid flux, for this. If you find a short, or if you find that the two boards are not connected right, use lots of flux and reflow the pin well.

Now follow these instructions to update the firmware : https://github.com/marchingband/wvr_usb_backpack

Youre done!
