#include <Arduino.h>
#include <Adafruit_TinyUSB.h>
#include <MIDI.h>
#include "jupx_iface/cc_to_memory.h"
#include "jupx_iface/jupx_headers.h"
#include "jupx_iface/macros.h"
#include "jupx_iface/memory_addresses.h"

// config
#define DEBUG_PORT Serial
#define DEBUG_INTERVAL 2000
// change this to midiUSB in case you use that to write to the Jupiter
#define SYSEX_TARGET midiDIN


#define JUPITER_XM
#define DEVICE_ID 17 // may change depending on your setup
#define SYSEX_DEVICE_ID ((DEVICE_ID)-1) // Roland can't count in hex...?
#define TOTAL_INT_SLOTS   12  // number of uint32_t parameters handled (max)
#define TOTAL_SHORT_SLOTS  0  // ditto for uint16_t
#define TOTAL_BYTE_SLOTS   0  // ditto for uint8_t
// subsequent parameters
#ifdef JUPITER_XM
#define MODEL_ID_SHORT 0x65
#else
#define MODEL_ID_SHORT 0x52
#endif

#define MODEL_ID 0, 0, 0, (MODEL_ID_SHORT)

// MIDI objects

Adafruit_USBD_MIDI usb_midi;
MIDI_CREATE_INSTANCE(Adafruit_USBD_MIDI, usb_midi, midiUSB);
MIDI_CREATE_INSTANCE(HardwareSerial, Serial1, midiDIN);


// globals
char mfgstr[32] = "Matcha";
char prodstr[32] = "JupX Forwarder";

ccHandlersView* ccHandler;

midiReaderState *         mrState;
realtimeControllerState * rtState;
memoryValuesState *       jupiMem;
slotToOffsetEntry *       intSlotsToOffsets;
slotToOffsetEntry *       shortSlotsToOffsets;
slotToOffsetEntry *       byteSlotsToOffsets;

readValue* readValues;
readValue* readValuesStartingPos;


// debug message buffer
char debugMessage[100] = "uninitialized";
char oldDebugMessage[100] = "uninitialized";

// slot registration config
BEGIN_SLOTS_DECLARATION
INT_SLOT(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_CUTOFF_FREQ, 0)
INT_SLOT(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_FREQ, 1)
INT_SLOT(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_CUTOFF_FREQUENCY, 2)
INT_SLOT(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_RESONANCE, 3)
INT_SLOT(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_RESO, 4)
INT_SLOT(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_RESONANCE, 5) 
END_SLOTS_DECLARATION



void doDebug(const char* message) {
  DEBUG_PORT.println(message);
}

void cutoffXResoHandler(uint8_t changedAmount) {
  /*
  INT_PARAM_HANDLER(tvfCutOffJpx, 0, TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_CUTOFF_FREQ, 0, 1023, 1);
  INT_PARAM_HANDLER(tvfCutOffAnl, 1, TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_FREQ, 0, 1023, 1);
  INT_PARAM_HANDLER(tvfCutOffZcr, 2, TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_CUTOFF_FREQUENCY, 0, 1023, 1);
  INT_PARAM_HANDLER(tvfResoJpx,   3, TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_RESONANCE, 0, 1023, -1);
  INT_PARAM_HANDLER(tvfResoAnl,   4, TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_RESO, 0, 1023, -1);
  INT_PARAM_HANDLER(tvfResoZcr,   5, TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_RESONANCE, 0, 1023, -1);
  */
  INT2_PARAM_HANDLER(tvfAndResoJpx0, 0, 1, TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_CUTOFF_FREQ, 4, 0, 1023, 0, 1023, 4, -4);
  INT2_PARAM_HANDLER(tvfAndResoAnl0, 2, 3, TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_FREQ, 4, 0, 1023, 0, 1023, 4, -4);
  INT2_PARAM_HANDLER(tvfAndResoZcr0, 4, 5, TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_CUTOFF_FREQUENCY, 4, 0, 1023, 0, 1023, 4, -4);
  // snprintf(debugMessage, 100, "cutoff changed by %d\n", changedAmount);
}

void nrpnMSBAddrHandler(uint8_t value) {
  mrState->nrpnAddress = ((value & 0x7f) << 8) & 0xff00;
  // DEBUG_PORT.printf("MSB addr: %02x ", value);
}

void nrpnLSBAddrHandler(uint8_t value) {
  mrState->nrpnAddress += (value & 0x7f);
  // DEBUG_PORT.printf("LSB addr: %02x ", value);
}

void nrpnDataMSBHandler(uint8_t value) {
  mrState->nrpnValue = ((value & 0x7f) << 7) & 0b11111110000000;
  // DEBUG_PORT.printf("MSB val: %02x ", value);
}

void nrpnDataLSBHandler(uint8_t value) {
  mrState->nrpnValue += (value & 0x7f);
  // DEBUG_PORT.printf("LSB val: %02x \n", value);
  handleNRPNEntry();
}


void handleNRPNEntry() {
  uint32_t valueToSend;
  if (rtState->cooldown > 0) return;
  switch (mrState->nrpnAddress) {
    case 0x0100:
      valueToSend = mrState->nrpnValue / 16;  // it goes from 0 to 1023
      // TODO: set value for relative change
      emitRolandDT1_int(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_CUTOFF_FREQ, &valueToSend);
      emitRolandDT1_int(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_FREQ, &valueToSend);
      emitRolandDT1_int(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_CUTOFF_FREQUENCY, &valueToSend);
      rtState->cooldown = 20;
      snprintf(debugMessage, 100, "we handled NRPN %04x and sent value %d (based on NRPN val %04x)\n", mrState->nrpnAddress, valueToSend, mrState->nrpnValue);
      break;
    case 0x0200:
      valueToSend = mrState->nrpnValue / 16;  // it goes from 0 to 1023
      // TODO: set value for relative change
      emitRolandDT1_int(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_RESONANCE, &valueToSend);
      emitRolandDT1_int(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_RESO, &valueToSend);
      emitRolandDT1_int(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_RESONANCE, &valueToSend);
      rtState->cooldown = 20;
      snprintf(debugMessage, 100, "we handled NRPN %04x and sent value %d (based on NRPN val %04x)\n", mrState->nrpnAddress, valueToSend, mrState->nrpnValue);
      break;
    default:
      snprintf(debugMessage, 100, "Unknown NRPN %04x for value %04x\n", mrState->nrpnAddress, mrState->nrpnValue);
  }
  mrState->nrpnAddress = 0;
  mrState->nrpnValue = 0;
}

void initializeCcHandler() {
  ccHandler = (ccHandlersView*)malloc(sizeof(ccHandlersView));
  for (int i = 0; i < 128; i++) {
    ccHandler->array.handlers[i] = NULL;
  }
  // now we can set a handler for each CC number
  SET_HANDLER(71, cutoffXResoHandler);


  // finally, handle the RPN/NRPN system
}

void initializeDT1Reader() {
  readValues = (readValue*)malloc(sizeof(readValue) * 128);
  readValuesStartingPos = readValues;
  memset(readValues, 0, sizeof(readValue) * 128);
  /*
  readValue* rv = readValues;
  for (int i = 0; i < 128; i++) {
    rv->offset = 0;
    rv->value = 0;
    rv++;
  }*/
}

void setReadValue(uint32_t offset, uint32_t value) {
  readValue* rv = readValuesStartingPos;
  for (uint8_t i = 0; i < 128; i++) {
    if (rv->offset == 0 || rv->offset == offset) {
      rv->offset = offset;
      rv->value = value;
      DEBUG_PORT.printf("Read value at position %d for offset %08x: %u\n", i, offset, value);
      break;
    }
    rv++;
  }
  slotToOffsetEntry * iO = intSlotsToOffsets;
  for (int i = 0; i < MAX(1, TOTAL_INT_SLOTS); i++) {
    if (iO->offset == offset) {
      jupiMem->intSlots[i] = value;
      break;
    }
  }
}

uint32_t getReadValue(uint32_t offset) {
  readValue* rv = readValuesStartingPos;
  mrState->mcErrno = NO_ERR;
  for (int i = 0; i < 128; i++) {
    if (rv->offset == offset) return rv->value;
    rv++;
  }
  mrState->mcErrno = ERR_VALUE_NOT_FOUND;
  return 0;
}




#pragma inline
void setupState() {
  mrState = (midiReaderState*) malloc(sizeof(midiReaderState));
  rtState = (realtimeControllerState*) malloc(sizeof(realtimeControllerState));
  jupiMem = (memoryValuesState *) malloc(sizeof(memoryValuesState));
  
  mrState->nrpnAddress = 0;
  mrState->nrpnValue = 0;
  mrState->mcErrno = 0;
  memset(rtState, 0, sizeof(realtimeControllerState)); // set the bitfield to everywhere
  jupiMem->byteSlots = (uint8_t *) malloc(sizeof(uint8_t) * max(1, TOTAL_BYTE_SLOTS));
  jupiMem->shortSlots = (uint16_t *) malloc(sizeof(uint16_t) * max(1, TOTAL_SHORT_SLOTS));
  jupiMem->intSlots = (uint32_t *) malloc(sizeof(uint32_t) * max(1, TOTAL_INT_SLOTS));
  intSlotsToOffsets = (slotToOffsetEntry*) malloc(sizeof(slotToOffsetEntry) * max(1, TOTAL_INT_SLOTS));
  shortSlotsToOffsets = (slotToOffsetEntry*) malloc(sizeof(slotToOffsetEntry) * max(1, TOTAL_SHORT_SLOTS));
  byteSlotsToOffsets = (slotToOffsetEntry*) malloc(sizeof(slotToOffsetEntry) * max(1, TOTAL_BYTE_SLOTS));\
  memset(jupiMem->byteSlots, 0, sizeof(uint8_t) * max(1, TOTAL_BYTE_SLOTS));
  memset(jupiMem->shortSlots, 0, sizeof(uint16_t) * max(1, TOTAL_SHORT_SLOTS));
  memset(jupiMem->intSlots, 0, sizeof(uint32_t) * max(1, TOTAL_INT_SLOTS));
  memset(intSlotsToOffsets, 0, sizeof(slotToOffsetEntry) * max(1, TOTAL_INT_SLOTS));
  memset(shortSlotsToOffsets, 0, sizeof(slotToOffsetEntry) * max(1, TOTAL_SHORT_SLOTS));
  memset(byteSlotsToOffsets, 0, sizeof(slotToOffsetEntry) * max(1, TOTAL_BYTE_SLOTS));
  _register_parameter_slots();
}



void decodeSysExDT1(byte* array, unsigned size) {
  debug_sysex(array, "decode-dt1", size);
  if (array[0] != 0xf0 || array[size - 1] != 0xf7) {
    doDebug("Wrong sysex header/tail");
    snprintf(debugMessage, 40, "Wrong sysex header/tail 0x%x/0x%x", array[0], array[size - 1]);
  }
  if (array[7] != 0x12) {
    snprintf(debugMessage, 40, "Command is not DT1 = 0x12: %x", array[6]);
  }
  if (size != 16) {
    snprintf(&(debugMessage[40]), 40, "Wrong message size %d != 16\n", size);
  }
  uint32_t offset = NIBS_TO_UINT32(array[8], array[9], array[10], array[11]);
  uint32_t value = NIBS_TO_UINT32(array[12], array[13], array[14], array[15]);
  uint8_t checksum = computeRolandChecksumForInt(offset, &value);
  if (checksum != array[16]) {
    snprintf(debugMessage, 100, "Wrong checksum for offset %x and value %x: %d (theoretical)!= %d (received)\n", offset, value, checksum, checksum, array[16]);
  }
  setReadValue(offset, value);
}

void handleSysExIncoming(byte* array, unsigned size) {
  // snprintf(debugMessage, 100, "sysex %x", array);
  debug_sysex(array, "handle-sysex", size);
  // return;
  if (array[1] != VENDOR_ID) {
    return;
    // snprintf(debugMessage, 40, "Wrong VENDOR_ID 0x%x", array[1]);
  }
  if (array[2] != SYSEX_DEVICE_ID) {
    //snprintf(debugMessage, 40, "Wrong DEVICE_ID 0x%x != 0x%x", array[2], SYSEX_DEVICE_ID);
    //doDebug("handle-sysexincoming - wrong device_id");
    return;
  }
  if (array[3] != 0 || array[4] != 0 || array[5] != 0 || array[6] != MODEL_ID_SHORT) {
    // snprintf(debugMessage, 40, "Wrong MODEL_ID 0x%x 0x%x 0x%x 0x%x", array[3], array[4], array[5], array[6]);
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

void setRolandChecksum(byte* sysex, uint8_t startingPos, uint8_t payloadLength) {
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

void debug_sysex(byte* sysex, char* source, uint8_t len) {
  DEBUG_PORT.printf("%s - Sysex message debug [len: %d]:", source, len);
  for (uint8_t i = 0; i < len; i++) {
    DEBUG_PORT.printf(" %02x", sysex[i]);
  }
  DEBUG_PORT.println(".");
}

void debug_sysex_quiet(byte* sysex, char* source, uint8_t len) {
  snprintf(debugMessage, 20, "%s - Sysex message debug:", source);

  for (uint8_t i = 0; i < len; i++) {
    snprintf(&debugMessage[20 + 3 * i], 3, " %02x", sysex[i]);
  }
  snprintf(&debugMessage[20 + 3 * len], 1, "\n");
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
  // midiUSB.sendSysEx(16, sysex);
  // DEBUG_PORT.println("Sent sysex RQ1/int");
}

// useful if you want to write 2 consecutive parameters with 2 separate values
void emitRolandDT1_2ints(const uint32_t offset, const uint32_t* value1, const uint32_t* value2) {
  static byte sysex[20] = { VENDOR_ID, (DEVICE_ID - 1), MODEL_ID, 0x11, 1, 2, 3, 4, 5, 6, 7, 8, 9, 6, 7, 8, 9 };
  memset(&sysex[7], offset, 4);  // takes 7,8,9,10
  memset(&sysex[11], NIB(*value1, 4), 1);
  memset(&sysex[12], NIB(*value1, 3), 1);
  memset(&sysex[13], NIB(*value1, 2), 1);
  memset(&sysex[14], NIB(*value1, 1), 1);
  memset(&sysex[15], NIB(*value2, 4), 1);
  memset(&sysex[16], NIB(*value2, 3), 1);
  memset(&sysex[17], NIB(*value2, 2), 1);
  memset(&sysex[18], NIB(*value2, 1), 1);
  setRolandChecksum(sysex, 7, 12);  // starts at position 7, lasts for 4 (offset) + 4 (val1) + 4 (val2) bytes
  SYSEX_TARGET.sendSysEx(20, sysex);
  debug_sysex_quiet(sysex, "emit-dt1-2ints", 20);
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

long milestone = DEBUG_INTERVAL;
bool midiInitialPollDone = false;


void initialMidiPolling() {
  DEBUG_PORT.println("initialMidiPolling: start");
  return;
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_RESONANCE);       // tvfResonance 3 
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_RESO);  // tvfResonance 4
  delay(100);
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_JUPITER_X_0_MDLJPX_FILTER_CUTOFF_FREQ);      // tvfCutOff 0
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_FILTER_FREQ);   // tvfCutOff 1
  delay(100);
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_CUTOFF_FREQUENCY);  // tvfCutOff 2 
  LOAD_INT_PARAM_VALUE(TEMPORARY_TONE_ZCORE_0_TONE_PARTIAL_TVF_RESONANCE);        // tvfResonance 5
  DEBUG_PORT.println("initialMidiPolling: end");
}


unsigned long nextDebug = 0;
uint8_t setupDone = 0;
uint8_t setup1Done = 0;
void setup1() {
  delay(4000);

  DEBUG_PORT.println("setup1: starting");
  pinMode(LED_BUILTIN, OUTPUT);
  if (!USBDevice.isInitialized()) {
    USBDevice.begin(0);
  }
  USBDevice.setManufacturerDescriptor(mfgstr);
  USBDevice.setProductDescriptor(prodstr);


  midiUSB.begin(MIDI_CHANNEL_OMNI);
  midiDIN.begin(MIDI_CHANNEL_OMNI);
  midiUSB.turnThruOff();
  midiDIN.turnThruOff();
  if (USBDevice.mounted()) {
    USBDevice.detach();
    delay(10);
    USBDevice.attach();
  }
  
  midiUSB.setHandleControlChange(handleCC);
  // midiDIN.setHandleControlChange(handleCC);
  midiDIN.setHandleSystemExclusive(handleSysExIncoming);
  DEBUG_PORT.println("setup1: done");
  setup1Done = 1;
}

void setup() {
  DEBUG_PORT.begin(115200);
  while (!DEBUG_PORT) ;
  DEBUG_PORT.println("setup: starting");
  initializeCcHandler();
  initializeDT1Reader();
  setupState();
  DEBUG_PORT.println("setup: done");
  setupDone = 1;
}

void loop1() {
  if (setup1Done + setupDone != 2) return;
#ifdef TINYUSB_NEED_POLLING_TASK
  USBDevice.task();
#endif
  midiUSB.read();
  midiDIN.read();
  if (rtState->cooldown > 5000) rtState->cooldown = 0;
  if (rtState->cooldown != 0) rtState->cooldown -= 1;
}

void loop() {
  if (setup1Done + setupDone != 2) return;
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
    nextDebug = millis() + DEBUG_INTERVAL;
  }
  if (DEBUG_PORT.available() > 0) {
    String mesg = DEBUG_PORT.readStringUntil('\n');
    DEBUG_PORT.printf("Reacting to message [%s]\n", mesg);
    if (mesg == "dump") {
      DEBUG_PORT.printf("Debug message: %s\nPrevious debug message: %s\n", debugMessage, oldDebugMessage);
      DEBUG_PORT.printf("Int Slots\n");
      slotToOffsetEntry * i_o = intSlotsToOffsets;
      readValue* rv = readValuesStartingPos;
      for (int i = 0; i < MAX(1, TOTAL_INT_SLOTS); i++) {
        DEBUG_PORT.printf("Slot %d/Offset %08x:\t%u\n", i_o->slot, i_o->offset, jupiMem->intSlots[i]);
        i_o++;
      }
      DEBUG_PORT.printf("Read Values\n");
      for (int i = 0; i < 128; i++) {
        if (rv->offset == 0) {
          rv++;
          continue;
        }
        DEBUG_PORT.printf("RV slot %d/offset %08x = %u\n", i, rv->offset, rv->value);
        rv++;
      }
    }
  }
}