/*
  midXparser
  A small footprint midi parser.
  Copyright (C) 2017/2018 by The KikGen labs.
  HEADER CLASS FILE - METHODS
  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.
  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
   Licence : MIT.
*/

#include "midiXparser.h"
#include <stdio.h>
#include <stdint.h>

const  uint8_t midiXparser::m_systemCommonMsglen[]={
        // SYSTEM COMMON
        0, // soxStatus             = 0XF0,
        2, // midiTimeCodeStatus    = 0XF1,
        3, // songPosPointerStatus  = 0XF2,
        2, // songSelectStatus      = 0XF3,
        0, // reserved1Status       = 0XF4,
        0, // reserved2Status       = 0XF5,
        1, // tuneRequestStatus     = 0XF6,
        0  // eoxStatus             = 0XF7,
};

const  uint8_t midiXparser::m_channelVoiceMsgMsglen[]={
        3, // noteOffStatus         = 0X80,
        3, // noteOnStatus          = 0X90,
        3, // polyKeyPressureStatus = 0XA0,
        3, // controlChangeStatus   = 0XB0,
        2, // programChangeStatus   = 0XC0,
        2, // channelPressureStatus = 0XD0,
        3, // pitchBendStatus       = 0XE0,
};

// Constructors
midiXparser::midiXparser() {

};

// Give the current sysex state,
bool midiXparser::isSysExMode() { return m_sysExMode ;}

bool midiXparser::wasSysExMode() {
  return  ( ( m_readByte == eoxStatus && m_isByteCaptured ) || m_sysExError );
}

// Give the status of the last SYSEX transmission.
bool midiXparser::isSysExError() { return m_sysExError ;}

// Used to check if the last byte parsed was captured
bool midiXparser::isByteCaptured() { return m_isByteCaptured; }

// Check the midi status of  a non sysex parsed midi msg
bool midiXparser::isMidiStatus(midiStatusValue midiStatus) {

  if ( m_midiParsedMsgType == noneMsgTypeMsk ) return false;  // Only if a parsed msg exists

  // Channel voice msg
  if ( m_midiParsedMsgType == channelVoiceMsgTypeMsk ) return ( ( m_midiMsg[0] & 0xF0 ) == (uint8_t)midiStatus ) ;
  // System Common msg
  if ( m_midiParsedMsgType == systemCommonMsgTypeMsk ) return ( m_midiMsg[0] == (uint8_t)midiStatus ) ;
  // realtime msg
  if ( m_midiParsedMsgType == realTimeMsgTypeMsk ) return ( m_midiMsgRealTime == (uint8_t)midiStatus) ;

  return false;
}

// Return the type of the last parsed midi message
uint8_t  midiXparser::getMidiMsgType() { return m_midiParsedMsgType; }

// Return the type of the currently parsed midi message
uint8_t  midiXparser::getMidiCurrentMsgType() { return m_midiCurrentMsgType; }

// Return the type of a midi status (cf enum)
uint8_t  midiXparser::getMidiStatusMsgTypeMsk(uint8_t midiStatus) {

  if (midiStatus >= 0xF8 ) return realTimeMsgTypeMsk;
  if (midiStatus == 0XF7 || midiStatus == 0xF0 ) return sysExMsgTypeMsk;
  if (midiStatus >= 0xF0 ) return systemCommonMsgTypeMsk;
  if (midiStatus >= 0x80 ) return channelVoiceMsgTypeMsk;

  return noneMsgTypeMsk ;
}

// Return the len of the last parsed midi message, including sysex
uint8_t  midiXparser::getMidiMsgLen() {
  if (m_midiParsedMsgType == sysExMsgTypeMsk )         return getSysExMsgLen() ;
  if (m_midiParsedMsgType == realTimeMsgTypeMsk )      return 1 ;
  if (m_midiParsedMsgType == channelVoiceMsgTypeMsk )  return m_channelVoiceMsgMsglen[ (getMidiMsg()[0] >> 4) - 8 ] ;
  if (m_midiParsedMsgType == systemCommonMsgTypeMsk )  return m_systemCommonMsglen[getMidiMsg()[0] & 0x0F] ;

  return 0;
}

// Return the len of a midistatus message (cf enum)
// Nb: SOX or EOX return always 0.

uint8_t midiXparser::getMidiStatusMsgLen(uint8_t midiStatus) {
  if (midiStatus >= 0xF8 ) return 1;
  if (midiStatus >= 0xF0 ) return m_systemCommonMsglen[midiStatus & 0x0F];
  if (midiStatus >= 0x80 ) return m_channelVoiceMsgMsglen[ (midiStatus >> 4) - 8 ];
  return 0 ;
}

// Return the len of the last Sysex msg.
// This persists until the next sysex.
unsigned midiXparser::getSysExMsgLen() { return m_sysExMsgLen ;}

// Return the parsed message buffered
uint8_t * midiXparser::getMidiMsg() {

    switch (m_midiParsedMsgType) {
      case realTimeMsgTypeMsk:
        return &m_midiMsgRealTime;
        break;
      case sysExMsgTypeMsk:
        return (uint8_t*) NULL;
        break;
    }

   return m_midiMsg ;
}

// Get the last byte parsed
byte midiXparser::getByte() { return m_readByte ;}

// Set filter mask all/none for all midi Msg including Sysex
// For Sysex, the "on the fly" mode is activated by default
// To change that, you must call explicitly setSysExFilter again.
void midiXparser::setMidiMsgFilter(uint8_t midiMsgTypeFilterMsk) {
  m_midiMsgTypeFilterMsk = midiMsgTypeFilterMsk;
}

//////////////////////////////////////////////////////////////////////////////
// midiXParser MIDI PARSER
//----------------------------------------------------------------------------
// The main method.
// It parses midi byte per byte and return true if a message is matching filters.
// Set also the byte capture flag if a byte belong to a filtered message.
//////////////////////////////////////////////////////////////////////////////
bool midiXparser::parse(byte readByte) {

    // Store the passed byte so it can be sent back to serial
    // if not captured
    m_readByte = readByte;
    m_isByteCaptured = false;
    m_sysExError = false;    // Clear any previous sysex error

    // MIDI Message status are starting at 0x80
    if ( readByte >= 0x80  )
    {

      // Check filters.
      m_isByteCaptured = (m_midiMsgTypeFilterMsk  & getMidiStatusMsgTypeMsk(readByte) );

      // SysEx can be terminated abnormally with a midi status.
      // We must check that before applying filters.
       if ( m_sysExMode && m_readByte < 0xF7 ) {
          m_sysExError = true;
          m_sysExMode = false;
          m_sysExMsgLen = m_sysExindexMsgLen;
       }

       // Real time messages must be processed as transparent for all other status
       if  ( readByte >= 0xF8 ) {
            m_midiParsedMsgType = realTimeMsgTypeMsk;
            // NB : m_midiCurrentMsgType can't be used as real time can be mixed with
            // channel voice msg.
            m_midiMsgRealTime = readByte;
            return m_isByteCaptured;
       }

       // Running status not possible at this point
       m_runningStatusPossible=false;

       // Reset current msg type and msg len
       m_midiCurrentMsgType = noneMsgTypeMsk;
       m_indexMsgLen = m_expectedMsgLen = 0;

       // Apply filter
       if (!m_isByteCaptured) return false;

       // Start SYSEX ---------------------------------------------------------

       // END OF Sysex.
       if ( readByte == eoxStatus ) {
            m_sysExMsgLen = m_sysExindexMsgLen;
            if (m_sysExMode ) {
               m_midiParsedMsgType = sysExMsgTypeMsk;
               m_sysExMode = false;
               return true;
            } // Isolated EOX without SOX.
            m_sysExMsgLen = 0;
            m_sysExError = true;
            m_midiCurrentMsgType = sysExMsgTypeMsk;
            return false;
       }

       // Start SYSEX
       if ( readByte == soxStatus ) {
              m_sysExMode = true;
              m_sysExindexMsgLen = 0;
              m_midiCurrentMsgType = sysExMsgTypeMsk;
              return false;
       }
       // Start midi msg ------------------------------------------------------

       // get the len of the midi msg to parse minus status
       m_midiMsg[0] = readByte;
       m_expectedMsgLen = getMidiStatusMsgLen(readByte) ;
       m_indexMsgLen = m_expectedMsgLen - 1;

      // Channel messages between 0x80 and 0xEF -------------------------------
      if ( readByte <= 0xEF ) {
          m_midiCurrentMsgType = channelVoiceMsgTypeMsk;
      }

      // System common messages between 0xF0 and 0xF7 -------------------------
      // but SOX / EOX
      else {
          m_midiCurrentMsgType = systemCommonMsgTypeMsk;
          // Case of 1 byte len midi msg (Tune request)
          if ( m_indexMsgLen == 0 ) {
            m_midiParsedMsgType = m_midiCurrentMsgType;
            return true;
          }
      }
    }

    // Midi Data from 00 to 0X7F ----------------------------------------------
    else {

          // Capture the SYSEX message if filter is set
          // If m_sysExBufferSize is 0, do not store
          if (m_sysExMode ) {
              m_sysExindexMsgLen++;
              m_isByteCaptured = true;
              return false;
          }

          // "Pure" midi message data
          // check if Running status set and if so, generate a true midi channel msg with
          // the previous one. Possible only if filters matchs.
          if (m_runningStatusPossible) {
                m_indexMsgLen = m_expectedMsgLen-1;
                m_runningStatusPossible = false;
          }

          // Len was set only if filters matched before
          if ( m_indexMsgLen ) {

            m_midiMsg[m_expectedMsgLen-m_indexMsgLen] = readByte;
            m_isByteCaptured = true;
            m_indexMsgLen -- ;
            // Message complete ?
            // Enable running status if it is a message channel.
            if (m_indexMsgLen == 0) {
                m_midiParsedMsgType = m_midiCurrentMsgType;
                if (m_midiParsedMsgType == channelVoiceMsgTypeMsk) {
                  m_runningStatusPossible = true;
                }
                return true;
            }
          }

     } // Midi data from 00 to 0X7F

     // All other data here are purely ignored.
     // In respect of the MIDI specifications.

    return false;
}