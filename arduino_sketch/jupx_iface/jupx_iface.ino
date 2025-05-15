#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "jupx_headers.h"

#define DEBUG_PORT Serial
#define CONTROLLED_BYTE_PARAMETER(offset, cc_num, range_min, range_max) \
  uint8_t parm_##cc_num##_value = 0; \
  static const ubyte_jupx_parameter parm_##cc_num = { (offset), (cc_num), &(parm_##cc_num##_value), (range_min), (range_max) }
#define SYSEX_TARGET midiDIN
#define CONTROLLED_UINT_PARAMETER(cc_num, val, range_min, range_max) \
  uint32_t parm_##cc_num##_value = 0; \
  static const uint_jupx_parameter parm_##cc_num = { (val), (cc_num), &(parm_##cc_num##_value), (range_min), (range_max) }
#define UINT32_TO_BYTES(val) (val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, (val & 0x000000ff)
#define SHORT_TO_BYTES(val) (val & 0xff00) >> 8, (val & 0xff)
#define NUMBER_TO_8NIBS(val) (val >> 28) & 0xf, (val >> 24) & 0xf, (val >> 20) & 0xf, (val >> 16) & 0xf, (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_6NIBS(val) (val >> 20) & 0xf, (val >> 16) & 0xf, (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_4NIBS(val) (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_3NIBS(val) (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_2NIBS(val) (val >> 4) & 0xf, (val)&0xf
#define NIB(val,i) (val >> ((i - 1) * 4)) & 0xf
#define BYTEPOS(val,i) ((val >> (i - 1) * 8)  & 0xff)


#define VENDOR_ID (0x41)
#define JUPITER_XM



// Config

char mfgstr[32] = "Matcha";
char prodstr[32] = "JupX Forwarder";
char debugMessage[100] = "uninitialized";
// Parameters
#define DEVICE_ID 17

#define CC71_ENABLED
CONTROLLED_UINT_PARAMETER(71, 0x0210004f, 0, 1023);



// subsequent parameters

#ifdef JUPITER_XM
#define MODEL_ID 0, 0, 0, 0x65
#else
#define MODEL_ID 0, 0, 0, 0x52
#endif
// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

// Create instance of Arduino MIDI library,
// and attach usb_midi as the transport.
// MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, midiUSB);
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, midiUSB);

// Create instance of Arduino MIDI library,
// and attach HardwareSerial Serial1 (TrinketM0 pins 3 & 4 or TX/RX on a QT Py, this is automatic)
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midiDIN);

void setup() {
  DEBUG_PORT.begin(115200);
  pinMode(LED_BUILTIN, OUTPUT);
  USBDevice.setManufacturerDescriptor(mfgstr);
  USBDevice.setProductDescriptor(prodstr);
  midiUSB.begin(MIDI_CHANNEL_OMNI);
  midiDIN.begin(MIDI_CHANNEL_OMNI);

  midiUSB.turnThruOff();
  midiDIN.turnThruOff();


  

}

void handleSysExIncoming(byte* array, unsigned size) {
  // snprintf(debugMessage, 100, "sysex %x", array);
  // debug_sysex(array, size);
  // return;
  digitalWrite(LED_BUILTIN, HIGH);
  delay(20);
  digitalWrite(LED_BUILTIN, LOW);

  DEBUG_PORT.printf("Got sysex with size %d\n", size);
  DEBUG_PORT.println("Data:");
  for (int i = 0; i < size; i++)
    DEBUG_PORT.printf("Data[%d]: %d\n", i, array[i]);
}


uint16_t sumIntegerBytes(const uint32_t number) {
  return (number & 0xff000000) >> 24 + (number & 0x00ff0000) >> 16 + (number & 0x0000ff00) >> 8 + (number & 0x000000ff);
}

uint16_t sumShortBytes(const uint16_t number) {
  return (number & 0xff00) >> 8 + (number & 0x00ff);
}

uint8_t computeRolandChecksumForByte(const uint32_t offset, const uint8_t value) {
  uint8_t r = 128 - ((BYTEPOS(offset, 4) + BYTEPOS(offset, 3) + BYTEPOS(offset, 2) + BYTEPOS(offset, 1) + value) % 128);
  return r == 128 ? 0 : r;
}

uint8_t computeRolandChecksumForShort(const uint32_t offset, const uint16_t* value) {
  uint8_t r = 128 - ((BYTEPOS(offset, 4) + BYTEPOS(offset, 3) + BYTEPOS(offset, 2) + BYTEPOS(offset, 1) + BYTEPOS(*value, 2) + BYTEPOS(*value, 1)) % 128);
  return r == 128 ? 0 : r;
}

uint8_t computeRolandChecksumForInt(const uint32_t offset, const uint32_t* value) {
  uint8_t r = 128 - ((BYTEPOS(offset, 4) + BYTEPOS(offset, 3) + BYTEPOS(offset, 2) + BYTEPOS(offset, 1) + BYTEPOS(*value, 4) + BYTEPOS(*value, 3) + BYTEPOS(*value, 2) + BYTEPOS(*value, 1)) % 128);
  return r == 128 ? 0 : r;
}
/*
void emitRolandDT1_byte(const uint32_t offset, const uint8_t* value) {
  // length: 13 ->  1 (Vendor) + 1 (Device ID) + 4 (Model ID) + 1 (command) + 4 (address) + 1 (value) + 1 (checksum)
  // static const byte sysex[13]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, UINT32_TO_BYTES(offset), value, computeRolandChecksumForByte(offset, value) };
  static byte sysex[13]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x12, 0, 0, 0, 0, 0, 0 };
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 1), 1);
  memset(&sysex[12], computeRolandChecksumForByte(offset, value), 1);4
  SYSEX_TARGET.sendSysEx(13, sysex);
  DEBUG_PORT.println("Sent sysex DT1/byte");
  
}

void emitRolandDT1_short(const uint32_t offset, const uint16_t* value) {
  // length: 14 ->  1 (Vendor) + 1 (Device ID) + 4 (Model ID) + 1 (command) + 4 (address) + 2 (value) + 1 (checksum)
  // static byte sysex[14]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, UINT32_TO_BYTES(offset), NUMBER_TO_2NIBS(*value), computeRolandChecksumForByte(offset, value) };
  static byte sysex[14]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x12, 0, 0, 0, 0, 0, 0, 0 };
0  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 2), 1);
  memset(&sysex[12], NIB(*value, 1), 1);
  memset(&sysex[13], computeRolandChecksumForShort(offset, value), 1);
  SYSEX_TARGET.sendSysEx(14, sysex);
  DEBUG_PORT.println("Sent sysex DT1/short");
  
}
*/
void emitRolandDT1_int(const uint32_t offset, const uint32_t* value) {
  // length: 16 ->  1 (Vendor) + 1 (Device ID) + 4 (Model ID) + 1 (command) + 4 (address) + 4 (value) + 1 (checksum)
  // static const byte sysex[16]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, UINT32_TO_BYTES(offset), NUMBER_TO_4NIBS(*value), computeRolandChecksumForByte(offset, value) };
  static byte sysex[16] = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x12, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 4), 1);
  memset(&sysex[12], NIB(*value, 3), 1);
  memset(&sysex[13], NIB(*value, 2), 1);
  memset(&sysex[14], NIB(*value, 1), 1);
  memset(&sysex[15], computeRolandChecksumForInt(offset, value), 1);
  debug_sysex(sysex, 16);

  SYSEX_TARGET.sendSysEx(16, sysex);
  DEBUG_PORT.println("Sent sysex DT1/int");
}

void debug_sysex(byte* sysex, uint8_t len) {
  DEBUG_PORT.println("Sysex message debug");
  for (uint8_t i = 0; i < len; i++) {
    DEBUG_PORT.printf("Byte %d: 0x%02x\n", i, sysex[i]);
  }
}

void emitRolandRQ1_int(const uint32_t offset, uint32_t* result, byte size) {
  DEBUG_PORT.printf("RQ1 on 0x%08x of size %d, writing to %p\n", offset, size, result);
  static byte sysex[16] = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(size, 4), 1);
  memset(&sysex[12], NIB(size, 3), 1);
  memset(&sysex[13], NIB(size, 2), 1);
  memset(&sysex[14], NIB(size, 1), 1);
  memset(&sysex[15], computeRolandChecksumForByte(offset, size), 1);
  debug_sysex(sysex, 16);
  SYSEX_TARGET.sendSysEx(16, sysex);
  midiUSB.sendSysEx(16, sysex);
  DEBUG_PORT.println("Sent sysex RQ1/int");
}

void handleCC(byte channel, byte number, byte value) {
  // DEBUG_PORT.printf("CC %d, value %d\n", number, value);
  //digitalWrite(LED_BUILTIN, HIGH);
  // delay(20);
  //digitalWrite(LED_BUILTIN, LOW);
  int8_t increment = value >= 64 ? 1 : -1;

  switch (number) {
    case 71:
      *(parm_71.current_value) += increment;
      if (*(parm_71.current_value) < parm_71.range_min) {
        *(parm_71.current_value) = parm_71.range_min;
      } else if (*(parm_71.current_value) > parm_71.range_max) {
        *(parm_71.current_value) = parm_71.range_max;
      }
      emitRolandDT1_int(parm_71.offset, parm_71.current_value);
      snprintf(debugMessage, 100, "CC %d with value %d, reaching new value %d\n", number, value, *(parm_71.current_value));
      break;
    default:
    snprintf(debugMessage, 100, "UNKNOWN CC %d with value %d\n", number, value);
      break;
  }
}

void handleCC2(byte channel, byte number, byte value) {
  // DEBUG_PORT.printf("CC %d, value %d\n", number, value);
  //digitalWrite(LED_BUILTIN, HIGH);
  // delay(20);
  //digitalWrite(LED_BUILTIN, LOW);
  int8_t increment = -64 + value;

  switch (number) {
    case 71:
      *(parm_71.current_value) += increment;
      if (*(parm_71.current_value) < parm_71.range_min) {
        *(parm_71.current_value) = parm_71.range_min;
      } else if (*(parm_71.current_value) > parm_71.range_max) {
        *(parm_71.current_value) = parm_71.range_max;
      }
      emitRolandDT1_int(parm_71.offset, parm_71.current_value);
      snprintf(debugMessage, 100, "CC2 %d with value %d, reaching new value %d\n", number, value, *(parm_71.current_value));
      break;
    default:
    snprintf(debugMessage, 100, "UNKNOWN CC %d with value %d\n", number, value);
      break;
  }
}




long milestone = 2000;


bool midiInitialPollDone = false;


void initialMidiPolling() {
  DEBUG_PORT.println("Requesting for parm 71");
  midiUSB.setHandleControlChange(handleCC);
  midiDIN.setHandleControlChange(handleCC2);
  midiDIN.setHandleSystemExclusive(handleSysExIncoming);
  emitRolandRQ1_int(parm_71.offset, parm_71.current_value, 4);
  midiDIN.sendNoteOn(64, 100, 1);
  delay(1000);
  midiDIN.sendNoteOn(63, 100, 1);
  midiDIN.sendNoteOff(64, 100, 1);
  delay(1000);
  midiDIN.sendNoteOff(63, 100, 1);
}

unsigned long nextDebug = 0;
void loop() {
  if (!midiInitialPollDone) {
    delay(3000);  
    initialMidiPolling();
    midiInitialPollDone = true;
  }
  // DEBUG_PORT.println("Well...");
  if (nextDebug < millis()) {
    
    DEBUG_PORT.printf("Debug message %lu:\t%s\n", nextDebug, debugMessage);
    nextDebug = millis() + 2000;
  }
  midiUSB.read();
  midiDIN.read();
}