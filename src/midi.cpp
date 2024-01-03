#include "midiXparser.h"
#include "esp_log.h"
#include "esp32-hal-log.h"
#include "midi_in.h"
#include "ws_log.h"
#include "midi.h"
#include "server.h"

midiXparser midiParser;
midiXparser usbMidiParser;
midiXparser webMidiParser;
uint8_t *msg;
uint8_t *usb_msg;
uint8_t *web_msg;
uint8_t sysex_byte;

void handle_sysex(void)
{
    switch(sysex_byte){
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
        default:
            log_e("got unkown sysex command");
            break;
    }
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
            if(len == 1)
                handle_sysex();
        }
    }
    else if(midiParser.isSysExMode() && midiParser.isByteCaptured()) // sysex data
    {
        sysex_byte = midiParser.getByte();
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
            if(len == 1)
                handle_sysex();
        }
    }
    else if(usbMidiParser.isSysExMode() && usbMidiParser.isByteCaptured()) // sysex data
    {
        sysex_byte = usbMidiParser.getByte();
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
            if(len == 1)
                handle_sysex();
        }
    }
    else if(webMidiParser.isSysExMode() && webMidiParser.isByteCaptured()) // sysex data
    {
        sysex_byte = webMidiParser.getByte();
    }
    return NULL;
}