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
#define SET_HANDLER(ccNumber, function) ccHandler->[(ccNumber)] = function


// must be called from within a function that has a changedAmount parameter reflecting
// the amount of change input by the user
#define INT_PARAM_HANDLER(slug,address,minRange,maxRange,direction) \
  static uint32_t parm ## slug ## value = 0; \
  parm ## slug ## value = min((maxRange), max((minRange), parm ## slug ## value + (changedAmount * (direction)))); \
  emitRolandDT1_int((address), (parm ## slug ## value))

#define SHORT_PARAM_HANDLER(slug,address,minRange,maxRange,direction) \
  static uint16_t parm ## slug ## value = 0; \
  parm ## slug ## value = min((maxRange), max((minRange), parm ## slug ## value + (changedAmount * (direction)))); \
  emitRolandDT1_short((address), (parm ## slug ## value))

#define BYTE_PARAM_HANDLER(slug,address,minRange,maxRange,direction) \
  static uint8_t parm ## slug ## value = 0; \
  parm ## slug ## value = min((maxRange), max((minRange), parm ## slug ## value + (changedAmount * (direction)))); \
  emitRolandDT1_byte((address), (parm ## slug ## value))
