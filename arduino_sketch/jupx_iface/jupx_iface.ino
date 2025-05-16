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
#define NO_ERR 0
#define ERR_VALUE_NOT_FOUND 1

#define SYSEX_DEVICE_ID ((DEVICE_ID) - 1)


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
char oldDebugMessage[100] = "uninitialized";
uint8_t mcErrno;

readValue* readValues;
readValue* readValuesStartingPos;
// USB MIDI object
Adafruit_USBD_MIDI usb_midi;

MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, midiUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midiDIN);

void doDebug(char * message) {
  DEBUG_PORT.println(message);
}

void cutoffHandler(uint8_t changedAmount) {
  INT_PARAM_HANDLER(tvfCutOff, 0x0210004f, 0, 1023, 1);
  INT_PARAM_HANDLER(tvfReso,   0x02100053, 0, 1023, -1);

  snprintf(debugMessage, 100, "cutoff changed by %d\n", changedAmount);
}

uint16_t nrpnAddr;
uint16_t nrpnVal;

void nrpnMSBAddrHandler(uint8_t value) {
  nrpnAddr = ((value & 0x7f) << 8) & 0xff00;
  // DEBUG_PORT.printf("MSB addr: %02x ", value);
}

void nrpnLSBAddrHandler(uint8_t value) {
  nrpnAddr += (value & 0x7f);
  // DEBUG_PORT.printf("LSB addr: %02x ", value);
}

void nrpnDataMSBHandler(uint8_t value) {
  nrpnVal = ((value & 0x7f) << 7) & 0b11111110000000;
  // DEBUG_PORT.printf("MSB val: %02x ", value);
}

void nrpnDataLSBHandler(uint8_t value) {
  nrpnVal += (value & 0x7f);
  // DEBUG_PORT.printf("LSB val: %02x \n", value);
  handleNRPNEntry();
}


void handleNRPNEntry() {
  uint32_t valueToSend;
  switch (nrpnAddr) {
    case 0x0100:
      valueToSend = nrpnVal / 16; // it goes from 0 to 1023
      emitRolandDT1_int(0x0210004f, &valueToSend);
      snprintf(debugMessage, 100, "we handled NRPN %04x and sent value %d (based on NRPN val %04x)\n", nrpnAddr, valueToSend, nrpnVal);
      break;
    case 0x0200:
      valueToSend = nrpnVal / 16; // it goes from 0 to 1023
      emitRolandDT1_int(0x02100053, &valueToSend);
      snprintf(debugMessage, 100, "we handled NRPN %04x and sent value %d (based on NRPN val %04x)\n", nrpnAddr, valueToSend, nrpnVal);
      break;
    default:
      snprintf(debugMessage, 100, "Unknown NRPN %04x for value %04x\n", nrpnAddr, nrpnVal);
  }
}

void initializeCcHandler() {
  ccHandler = (ccHandlersView*) malloc(sizeof(ccHandlersView));
  for (int i = 0; i < 128; i++) {
    ccHandler->array.handlers[i] = NULL;
  }
  // now we can set a handler for each CC number
  SET_HANDLER(71, cutoffHandler);


  // finally, handle the RPN/NRPN system

}

void initializeDT1Reader() {
  readValues = (readValue*) malloc(sizeof(readValue) * 128);
  readValuesStartingPos = readValues;
  readValue* rv = readValues;
  for (int i = 0; i < 128; i++) {
    rv->offset = 0;
    rv->value = 0;
    rv++;
  }
}

void setReadValue(uint32_t offset, uint32_t value) {
  readValue* rv = readValuesStartingPos;
  for (int i = 0; i < 128; i++) {
    if (rv->offset == 0 || rv->offset == offset) {
      rv->offset = offset;
      rv->value = value;
      break;
    }
    rv++;
  }
}

uint32_t getReadValue(uint32_t offset) {
  readValue* rv = readValuesStartingPos;
  mcErrno = NO_ERR;
  for (int i = 0; i < 128; i++) {
    if (rv->offset == offset) return rv->value;
    rv++;
  }
  mcErrno = ERR_VALUE_NOT_FOUND;
  return 0;
}


void setup() {
  DEBUG_PORT.begin(115200);
  mcErrno = 0;
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
  if (array[0] != 0xf0 || array[17] != 0xf7) {
    doDebug("Wrong sysex header/tail");
    snprintf(debugMessage, 40, "Wrong sysex header/tail 0x%x/0x%x", array[0], array[17]);
  }
  if (array[7] != 0x12) {
    snprintf(debugMessage, 40, "Command is not DT1 = 0x12: %x", array[6]);
  }
  if (size != 16) {
    snprintf(debugMessage, 40, "Wrong message size %d != 16\n", size);
  }
  snprintf(debugMessage, 100, "Reading DT1 for offset %x %x %x %x\n", array[8], array[9], array[10], array[11]);
  uint32_t offset = NIBS_TO_UINT32(array[8], array[9], array[10], array[11]);
  uint32_t value = NIBS_TO_UINT32(array[12], array[13], array[14], array[15]);
  uint8_t checksum = computeRolandChecksumForInt(offset, &value);
  if (checksum != array[16]) {
    snprintf(debugMessage, 100, "Wrong checksum for offset %x and value %x: %d (theoretical)!= %d (received)\n", offset, value, checksum, checksum, array[15]);
  }
  setReadValue(offset, value);
}

void handleSysExIncoming(byte* array, unsigned size) {
  // snprintf(debugMessage, 100, "sysex %x", array);
  debug_sysex(array, "handle-sysex", size);
  // return;
  if (array[1] != VENDOR_ID) {
    snprintf(debugMessage, 40, "Wrong VENDOR_ID 0x%x", array[1]);
    return;
  }
  if (array[2] != SYSEX_DEVICE_ID) {
    snprintf(debugMessage, 40, "Wrong DEVICE_ID 0x%x != 0x%x", array[2], SYSEX_DEVICE_ID);
    doDebug("handle-sysexincoming - wrong device_id");
    return;
  }
  if (array[3] != 0 || array[4] != 0 || array[5] != 0 || array[6] != MODEL_ID_SHORT) {
    snprintf(debugMessage, 40, "Wrong MODEL_ID 0x%x 0x%x 0x%x 0x%x", array[3], array[4], array[5], array[6]);
    return;
  }

  digitalWrite(LED_BUILTIN, HIGH);
  delay(20);
  digitalWrite(LED_BUILTIN, LOW);
  switch (array[7]) {
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

void setRolandChecksum(byte * sysex, uint8_t startingPos, uint8_t payloadLength) {
  uint8_t ck = 0;
  for (uint8_t i = startingPos; i < startingPos + payloadLength; i++) {
    
    ck = (ck + sysex[i]) & 0x7f;
  }
  sysex[startingPos + payloadLength] = (0x80 - ck) & 0x7f;
}

void emitRolandDT1_byte(const uint32_t offset, const uint8_t* value) {
  // length: 13 ->  1 (Vendor) + 1 (Device ID) + 4 (Model ID) + 1 (command) + 4 (address) + 1 (value) + 1 (checksum)
  // static const byte sysex[13]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, UINT32_TO_BYTES(offset), value, computeRolandChecksumForByte(offset, value) };
  static byte sysex[13] = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x12, 0, 0, 0, 0, 0, 0 };
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 1), 1);
  // memset(&sysex[12], computeRolandChecksumForByte(offset, *value), 1);
  setRolandChecksum(sysex, 7, 5);
  SYSEX_TARGET.sendSysEx(13, sysex);
  DEBUG_PORT.println("Sent sysex DT1/byte");
}

void emitRolandDT1_short(const uint32_t offset, const uint16_t* value) {
  // length: 14 ->  1 (Vendor) + 1 (Device ID) + 4 (Model ID) + 1 (command) + 4 (address) + 2 (value) + 1 (checksum)
  // static byte sysex[14]  = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, UINT32_TO_BYTES(offset), NUMBER_TO_2NIBS(*value), computeRolandChecksumForByte(offset, value) };
  static byte sysex[14] = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x12, 0, 0, 0, 0, 0, 0, 0 };
  memset(&sysex[7], BYTEPOS(offset, 4), 1);
  memset(&sysex[8], BYTEPOS(offset, 3), 1);
  memset(&sysex[9], BYTEPOS(offset, 2), 1);
  memset(&sysex[10], BYTEPOS(offset, 1), 1);
  memset(&sysex[11], NIB(*value, 2), 1);
  memset(&sysex[12], NIB(*value, 1), 1);
  // memset(&sysex[13], computeRolandChecksumForShort(offset, value), 1);
  setRolandChecksum(sysex, 7, 6);
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
  // memset(&sysex[15], computeRolandChecksumForInt(offset, value), 1);
  setRolandChecksum(sysex, 7, 8);
  // debug_sysex(sysex, "emit-DT1 (int)", 16);  
  SYSEX_TARGET.sendSysEx(16, sysex);
}

void debug_sysex(byte* sysex, char * source, uint8_t len) {
  DEBUG_PORT.printf("%s - Sysex message debug:", source);
  for (uint8_t i = 0; i < len; i++) {
    DEBUG_PORT.printf(" %02x", sysex[i]);
  }
  DEBUG_PORT.println(".");
}

void emitRolandRQ1_int(const uint32_t offset, byte size) {
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
  debug_sysex(sysex, "emit-RQ1", 16);
  SYSEX_TARGET.sendSysEx(16, sysex);
  midiUSB.sendSysEx(16, sysex);
  DEBUG_PORT.println("Sent sysex RQ1/int");
}


void handleCC(byte channel, byte number, byte value) {
  switch (number) {
    case 0x63:
      nrpnMSBAddrHandler(value);
      break;
    case 0x62:
      nrpnLSBAddrHandler(value);
      break;
    case 0x06:
      nrpnDataMSBHandler(value);
      break;
    case 0x26:
      nrpnDataLSBHandler(value);
      break;
    default:
      break;
  }
  // non-NRPN sorcery things are all relative
  if (ccHandler->array.handlers[number] == NULL) return;
  uint8_t increment = -64 + value;
  ccHandler->array.handlers[number](increment);
  // snprintf(debugMessage, 20, "CC[%d][%d]\n", number, value);
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
  LOAD_INT_PARAM_VALUE(0x0210004f); // tvfCutOff
  LOAD_INT_PARAM_VALUE(0x02100053); // tvfResonance
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
    if (strcmp(oldDebugMessage, debugMessage) != 0) {
      DEBUG_PORT.printf("Debug message %lu:\t%s\n", nextDebug, debugMessage);
      strcpy(oldDebugMessage, debugMessage);
    }
    nextDebug = millis() + 2000;
  }
  midiUSB.read();
  midiDIN.read();
}