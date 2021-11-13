#include "midiXparser.h"
#include "esp_log.h"
#include "esp32-hal-log.h"
#include "midi_in.h"
#include "ws_log.h"
#include "midi.h"

midiXparser midiParser;
midiXparser usbMidiParser;
uint8_t *msg;
uint8_t *usb_msg;


void midi_parser_init(void)
{
    midiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk );
    usbMidiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk );
}

extern "C" uint8_t* midi_parse(uint8_t in)
{
    if ( midiParser.parse( in ) )  // Do we received a channel voice msg ?
    {
        if ( 
                midiParser.isMidiStatus(midiXparser::noteOnStatus) || 
                midiParser.isMidiStatus(midiXparser::noteOffStatus) || 
                midiParser.isMidiStatus(midiXparser::programChangeStatus) || 
                midiParser.isMidiStatus(midiXparser::controlChangeStatus) 
            ) 
        {
            msg = midiParser.getMidiMsg();
            return msg;
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
                usbMidiParser.isMidiStatus(midiXparser::controlChangeStatus) 
            ) 
        {
            usb_msg = usbMidiParser.getMidiMsg();
            return usb_msg;
        }
    }
    return NULL;
}