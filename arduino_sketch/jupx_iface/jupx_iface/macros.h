#define UINT32_TO_BYTES(val) (val & 0xff000000) >> 24, (val & 0x00ff0000) >> 16, (val & 0x0000ff00) >> 8, (val & 0x000000ff)
#define SHORT_TO_BYTES(val) (val & 0xff00) >> 8, (val & 0xff)
#define NUMBER_TO_8NIBS(val) (val >> 28) & 0xf, (val >> 24) & 0xf, (val >> 20) & 0xf, (val >> 16) & 0xf, (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_6NIBS(val) (val >> 20) & 0xf, (val >> 16) & 0xf, (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_4NIBS(val) (val >> 12) & 0xf, (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_3NIBS(val) (val >> 8) & 0xf, (val >> 4) & 0xf, (val)&0xf
#define NUMBER_TO_2NIBS(val) (val >> 4) & 0xf, (val)&0xf
#define NIBS_TO_UINT32(nib1, nib2, nib3, nib4) nib4 + (nib3 << 8) + (nib2 << 16) + (nib1 << 24)
#define NIB(val,i) (val >> ((i - 1) * 4)) & 0xf
#define BYTEPOS(val,i) ((val >> (i - 1) * 8)  & 0xff)
#define SET_HANDLER(ccNumber,function) ccHandler->array.handlers[(ccNumber)] = function
#define NO_ERR 0
#define ERR_VALUE_NOT_FOUND 1
#define VENDOR_ID (0x41) // Roland


// must be called from within a function that has a changedAmount parameter reflecting
// the amount of change input by the user
#define INT_PARAM_HANDLER(slug,slot,address,minRange,maxRange,direction) \
  jupiMem->intSlots[(slot)] = min((maxRange), max((minRange), jupiMem->intSlots[(slot)] + (changedAmount * (direction))));\
  emitRolandDT1_int((address), &(jupiMem->intSlots[(slot)]))

#define INT2_PARAM_HANDLER(slug,slot1,slot2,address,offset,minRange1,maxRange1,minRange2,maxRange2,direction1,direction2) \
  jupiMem->intSlots[(slot1)] = min((maxRange1), max((minRange1), jupiMem->intSlots[(slot1)] + (changedAmount * (direction1))));\
  jupiMem->intSlots[(slot2)] = min((maxRange2), max((minRange2), jupiMem->intSlots[(slot2)] + (changedAmount * (direction2))));\
  emitRolandDT1_2ints((address), &(jupiMem->intSlots[(slot1)]), &(jupiMem->intSlots[(slot2)]))

#define LOAD_INT_PARAM_VALUE(address) emitRolandRQ1_int((address), 4)

#define SHORT_PARAM_HANDLER(slug,slot,address,minRange,maxRange,direction) \
  jupiMem->shortSlots[(slot)] = min((maxRange), max((minRange), jupiMem->shortSlots[(slot)] + (changedAmount * (direction))));\
  emitRolandDT1_short((address), &(jupiMem->shortSlots[(slot)]))

#define BYTE_PARAM_HANDLER(slug,slot,address,minRange,maxRange,direction) \
  jupiMem->byteSlots[(slot)] = min((maxRange), max((minRange), jupiMem->byteSlots[(slot)] + (changedAmount * (direction))));\
  emitRolandDT1_byte((address), &(jupiMem->shortSlots[(slot)]))


#define BEGIN_SLOTS_DECLARATION \
void _register_parameter_slots() {\
  DEBUG_PORT.println("** register parameter slots"); \
  slotToOffsetEntry* i_o = intSlotsToOffsets;\
  slotToOffsetEntry* s_o = shortSlotsToOffsets;\
  slotToOffsetEntry* b_o = byteSlotsToOffsets;

#define END_SLOTS_DECLARATION \
DEBUG_PORT.println("** end parameter slots"); \
}

#define INT_SLOT(_offset,_slot) \
  i_o->slot = (_slot); i_o->offset = (_offset); i_o++;

#define SHORT_SLOT(_offset,_slot) \
  s_o->slot = (_slot); s_o->offset = (_offset); s_o++;

#define BYTE_SLOT(_offset,_slot) \
  b_o->slot = (_slot); b_o->offset = (_offset); b_o++;
