/*
  midXparser
  A small footprint midi parser.
  Copyright (C) 2017/2018 by The KikGen labs.
  HEADER CLASS FILE
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
// #pragma once

#ifndef midiXparser_h
#define midiXparser_h

#if ARDUINO
#include <Arduino.h>
#else
#include <inttypes.h>
typedef uint8_t byte;
#endif
#include <stdlib.h>

class midiXparser {
  uint8_t  m_midiMsg[3];
  uint8_t  m_midiMsgRealTime; // Used for real time only
  uint8_t  m_indexMsgLen = 0;
  uint8_t  m_expectedMsgLen = 0;
  bool     m_sysExMode = false;
  bool     m_sysExError = false;
  unsigned m_sysExMsgLen = 0;
  unsigned m_sysExindexMsgLen = 0;
  bool     m_isByteCaptured=false;
  byte     m_readByte = 0;
  bool     m_runningStatusPossible=false;
  uint8_t  m_midiMsgTypeFilterMsk = noneMsgTypeMsk;
  uint8_t  m_midiParsedMsgType    = noneMsgTypeMsk;
  uint8_t  m_midiCurrentMsgType   = noneMsgTypeMsk;

  static const  uint8_t m_systemCommonMsglen[8];
  static const  uint8_t m_channelVoiceMsgMsglen[7];



  public:
    // Midi messages type
    enum midiMsgTypeMaskValue {
        noneMsgTypeMsk          = 0B0000,
        channelVoiceMsgTypeMsk  = 0B0001,
        systemCommonMsgTypeMsk  = 0B0010,
        realTimeMsgTypeMsk      = 0B0100,
        sysExMsgTypeMsk         = 0B1000,
        allMsgTypeMsk           = 0B1111
    };

  enum midiStatusValue {
        // CHANNEL VOICE
        noteOffStatus         = 0X80,
        noteOnStatus          = 0X90,
        polyKeyPressureStatus = 0XA0,
        controlChangeStatus   = 0XB0,
        programChangeStatus   = 0XC0,
        channelPressureStatus = 0XD0,
        pitchBendStatus       = 0XE0,
        // SYSTEM COMMON
        soxStatus             = 0XF0,
        midiTimeCodeStatus    = 0XF1,
        songPosPointerStatus  = 0XF2,
        songSelectStatus      = 0XF3,
        reserved1Status       = 0XF4,
        reserved2Status       = 0XF5,
        tuneRequestStatus     = 0XF6,
        eoxStatus             = 0XF7,
        // REAL TIME
        timingClockStatus     = 0XF8,
        reserved3Status       = 0XF9,
        startStatus           = 0XFA,
        continueStatus        = 0XFB,
        stopStatus            = 0XFC,
        reserved4Status       = 0XFD,
        activeSensingStatus   = 0XFE,
        systemResetStatus     = 0XFF
    };

    // Constructor
    midiXparser();

    // Methods
    bool        isSysExMode() ;
    bool        wasSysExMode() ;
    bool        isSysExError();
    bool        isByteCaptured() ;
    bool        isMidiStatus(midiStatusValue );
    uint8_t     getMidiMsgType() ;
    uint8_t     getMidiCurrentMsgType() ;
    uint8_t     getMidiMsgLen();
    uint8_t *   getMidiMsg();
    byte        getByte() ;
    unsigned    getSysExMsgLen() ;
    void        setMidiMsgFilter(uint8_t );
    bool        parse(byte );
    static uint8_t     getMidiStatusMsgTypeMsk(uint8_t ) ;
    static uint8_t     getMidiStatusMsgLen(uint8_t );

};



#endif