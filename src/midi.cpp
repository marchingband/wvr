#include "midiXparser.h"
#include "esp_log.h"
#include "esp32-hal-log.h"
#include "midi_in.h"
#include "ws_log.h"
#include "midi.h"

midiXparser midiParser;
uint8_t *msg;


void midi_parser_init(void)
{
    midiParser.setMidiMsgFilter( midiXparser::channelVoiceMsgTypeMsk );
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