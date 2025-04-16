#include "Arduino.h"
#include "midiXparser.h"
#include "esp_log.h"
#include "esp32-hal-log.h"
#include "midi_in.h"
#include "ws_log.h"
#include "midi.h"
#include "server.h"

#define SYSEX_LEN 4
#define WVR_SYSEX_VENDOR_ID 0x69

midiXparser midiParser;
midiXparser usbMidiParser;
midiXparser webMidiParser;
uint8_t *msg;
uint8_t *usb_msg;
uint8_t *web_msg;
uint8_t sysex_bytes[SYSEX_LEN];
uint8_t sysex_bytes_p = 0;

extern "C" {
    void set_global_volume(uint8_t vol);
}

void handle_sysex(uint8_t len)
{
    if(sysex_bytes[1] != WVR_SYSEX_VENDOR_ID){
        sysex_bytes_p = 0;
        return;
    }
    switch(sysex_bytes[2]){
        case 0x01: // WiFi on
            log_i("got sysex 0x01, wifi on");
            if(!get_wifi_is_on())
                server_resume();
            break;
        case 0x02: // WiFi off
            log_i("got sysex 0x02, wifi off");
            if(get_wifi_is_on())
                server_pause();
            break;
        case 0x03: // set global volume
            log_i("got sysex 0x03, set global volume to %d", sysex_bytes[3]);
            set_global_volume(sysex_bytes[3]);
            break;
        default:
            log_e("got unkown sysex command");
            break;
    }
    sysex_bytes_p = 0;
}

void midi_parser_init(void)
{
    midiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk | midiXparser::sysExMsgTypeMsk );
    usbMidiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk | midiXparser::sysExMsgTypeMsk );
    webMidiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk | midiXparser::sysExMsgTypeMsk );
}

extern "C" uint8_t* midi_parse(uint8_t in)
{
    if ( midiParser.parse( in ) )  // Do we received a channel voice msg ?
    {
        if ( 
                midiParser.isMidiStatus(midiXparser::noteOnStatus) || 
                midiParser.isMidiStatus(midiXparser::noteOffStatus) || 
                midiParser.isMidiStatus(midiXparser::programChangeStatus) || 
                midiParser.isMidiStatus(midiXparser::controlChangeStatus) ||
                midiParser.isMidiStatus(midiXparser::pitchBendStatus) 
            ) 
        {
            msg = midiParser.getMidiMsg();
            return msg;
        }
        else if(midiParser.getMidiMsgType() == midiXparser::sysExMsgTypeMsk) // sysex EOX
        {
            int len = midiParser.getSysExMsgLen();
            handle_sysex(len);
        }
    }
    else if(midiParser.isSysExMode() && midiParser.isByteCaptured()) // sysex data
    {
        uint8_t b = midiParser.getByte();
        if(sysex_bytes_p < SYSEX_LEN){
            sysex_bytes[sysex_bytes_p++] = b;
        }
    }
    return NULL;
}

extern "C" uint8_t* usb_midi_parse(uint8_t in)
{
    if ( usbMidiParser.parse( in ) )  // Do we received a channel voice msg ?
    {
        if ( 
                usbMidiParser.isMidiStatus(midiXparser::noteOnStatus) || 
                usbMidiParser.isMidiStatus(midiXparser::noteOffStatus) || 
                usbMidiParser.isMidiStatus(midiXparser::programChangeStatus) || 
                usbMidiParser.isMidiStatus(midiXparser::controlChangeStatus) ||
                usbMidiParser.isMidiStatus(midiXparser::pitchBendStatus) 
            ) 
        {
            usb_msg = usbMidiParser.getMidiMsg();
            return usb_msg;
        }
        else if(usbMidiParser.getMidiMsgType() == midiXparser::sysExMsgTypeMsk) // sysex EOX
        {
            int len = usbMidiParser.getSysExMsgLen();
            handle_sysex(len);
        }
    }
    else if(usbMidiParser.isSysExMode() && usbMidiParser.isByteCaptured()) // sysex data
    {
        if(sysex_bytes_p < SYSEX_LEN){
            sysex_bytes[sysex_bytes_p++] = usbMidiParser.getByte();
        }
    }
    return NULL;
}

extern "C" uint8_t* web_midi_parse(uint8_t in)
{
    if ( webMidiParser.parse( in ) )  // Do we received a channel voice msg ?
    {
        if ( 
                webMidiParser.isMidiStatus(midiXparser::noteOnStatus) || 
                webMidiParser.isMidiStatus(midiXparser::noteOffStatus) || 
                webMidiParser.isMidiStatus(midiXparser::programChangeStatus) || 
                webMidiParser.isMidiStatus(midiXparser::controlChangeStatus) ||
                webMidiParser.isMidiStatus(midiXparser::pitchBendStatus) 
            ) 
        {
            web_msg = webMidiParser.getMidiMsg();
            return web_msg;
        }
        else if(webMidiParser.getMidiMsgType() == midiXparser::sysExMsgTypeMsk) // sysex EOX
        {
            int len = webMidiParser.getSysExMsgLen();
            handle_sysex(len);

        }
    }
    else if(webMidiParser.isSysExMode() && webMidiParser.isByteCaptured()) // sysex data
    {
        if(sysex_bytes_p < SYSEX_LEN){
            sysex_bytes[sysex_bytes_p++] = usbMidiParser.getByte();
        }

    }
    return NULL;
}