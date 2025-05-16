#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "jupx_iface/cc_to_memory.h"
#include "jupx_iface/jupx_headers.h"
#include "jupx_iface/macros.h"

#define DEBUG_PORT Serial
#define SYSEX_TARGET midiDIN


#define VENDOR_ID (0x41)
#define JUPITER_XM
#define DEVICE_ID 17

ccHandlersView* ccHandler;
// Config

char mfgstr[32] = "Matcha";
char prodstr[32] = "JupX Forwarder";

// subsequent parameters

#ifdef JUPITER_XM
#define MODEL_ID_SHORT 0x65
#else
#define MODEL_ID_SHORT 0x52
#endif

#define MODEL_ID 0, 0, 0, (MODEL_ID_SHORT)

// debug message buffer
char debugMessage[100] = "uninitialized";


readValue * readValues;
readValue * readValuesStartingPos;
// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, midiUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midiDIN);

void cutoffHandler(int8_t changedAmount) {
  INT_PARAM_HANDLER("tvfCutOff", 0x0210004f, 0, 1023, 1);

  snprintf(debugMessage, 100, "cutoff changed by %d\n", changedAmount);
}

void initializeCcHandler() {
  ccHandler = malloc(sizeof(ccHandlersView));
  for (int i = 0; i < 128; i++) {
    ccHandler->array[i] = NULL;
  }
  // now we can set a handler for each CC number
  SET_HANDLER(71, cutoffHandler);
}

void initializeDT1Reader() {
  readValues = malloc(sizeof(readValue) * 128);
  readValuesStartingPos = readValues;
  readValue * rv = readValues;
  for (int i = 0; i < 128; i++) {
    rv->offset = 0;
    rv->value = 0;
    rv++;
  }
}

void setReadValue(offset, value) {
  readValue * rv = readValuesStartingPos;
  for (int i = 0; i < 128; i++) {
    if (rv->offset == 0 || rv->offset == offset) {
      rv->offset = offset;
      rv-> value = value;
      break;
    }
    rv++;
  }
}


void setup() {
  DEBUG_PORT.begin(115200);
  initializeCcHandler();
  initializeDT1Reader();
  pinMode(LED_BUILTIN, OUTPUT);
  USBDevice.setManufacturerDescriptor(mfgstr);
  USBDevice.setProductDescriptor(prodstr);
  midiUSB.begin(MIDI_CHANNEL_OMNI);
  midiDIN.begin(MIDI_CHANNEL_OMNI);

  midiUSB.turnThruOff();
  midiDIN.turnThruOff();
}

void decodeSysExDT1(byte* array, unsigned size) {

  if (array[6] != 0x12) {
    snprintf(debugMessage, "Command is not DT1 = 0x12: %x", array[6], 40);
  }
  if (size != 16) {
    snprintf(debugMessage, "Wrong message size %d != 16\n", size, 40);
  }
  snprintf(debugMessage, "Reading DT1 for offset %x %x %x %x\n", array[7], array[8], array[9], array[10]);
  uint32_t offset = NIBS_TO_UINT32(array[7], array[8], array[9], array[10]);
  uint32_t value = NIBS_TO_UINT32(array[11], array[12], array[13], array[14])
  uint8_t checksum = computeRolandChecksumForInt(offset, &value);
  if (checksum != array[15]) {
    snprintf("Wrong checksum for offset %x and value %x: %d (theoretical)!= %d (received)\n", offset, value, checksum, checksum, array[15], 100);
  }
  setReadValue(offset, value);
}

void handleSysExIncoming(byte* array, unsigned size) {
  // snprintf(debugMessage, 100, "sysex %x", array);
  // debug_sysex(array, size);
  // return;
  if (array[0] != VENDOR_ID) {
    snprintf(debugMessage, "Wrong VENDOR_ID %x", array[0], 40);
    return;
  }
  if (array[1] != DEVICE_ID) {
    snprintf(debugMessage, "Wrong DEVICE_ID %x", array[1], 40);
    return;
  }
  if (array[2] != 0 || array[3] != 0 || array[4] != 0 || array[5] != MODEL_ID_SHORT) {
    snprintf(debugMessage, "Wrong MODEL_ID %x % %x %x", array[2], array[3], array[4], array[5], 40);
    return;
  }

  digitalWrite(LED_BUILTIN, HIGH);
  delay(20);
  digitalWrite(LED_BUILTIN, LOW);
  switch (array[6]) {
    case 0x12:
      decodeSysExDT1(array, size);
      break;
    default:
      break;
  }
  
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
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 2), 1);
  memset(&sysex[12], NIB(*value, 1), 1);
  memset(&sysex[13], computeRolandChecksumForShort(offset, value), 1);
  SYSEX_TARGET.sendSysEx(14, sysex);
  DEBUG_PORT.println("Sent sysex DT1/short");
  
}

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





// size is always 4
// careful: this is disruptive of normal operation and must
// only be used at initialization
uint32_t obtainMemoryValue_int(const uint32_t offset) {
  uint32_t res;

}


void handleCC(byte channel, byte number, byte value) {
  if (ccHandler->array[number] == NULL) return;

  int8_t increment = -64 + value;
  ccHandler->array[number](increment);
  
  snprintf(debugMessage, 20, "CC[%d][%d]\n", number, value);
  }
}


// scan parameter presets to know which MIDI CC to respond to
void scanParameterPresets() {
  
}


long milestone = 2000;
bool midiInitialPollDone = false;


void initialMidiPolling() {
  DEBUG_PORT.println("Requesting for parm 71");
  midiUSB.setHandleControlChange(handleCC);
  // midiDIN.setHandleControlChange(handleCC);
  midiDIN.setHandleSystemExclusive(handleSysExIncoming);
  emitRolandRQ1_int(parm_71.offset, parm_71.current_value, 4);
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