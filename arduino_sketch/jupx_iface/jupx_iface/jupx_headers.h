typedef struct {
  uint32_t offset;
  uint8_t *current_value;
  uint8_t range_min;
  uint8_t range_max;

} ubyte_jupx_parameter;

typedef struct {
  uint32_t offset;
  uint16_t *current_value;
  uint16_t range_min;
  uint16_t range_max;
} ushort_jupx_parameter;

typedef struct {
  uint32_t offset;
  uint32_t *current_value;
  uint32_t range_min;
  uint32_t range_max;
} uint_jupx_parameter;

typedef struct {
  byte preset_type;
  void *preset;
} jupx_parameter_wrapper;

typedef struct {
  uint8_t settings_count;
  jupx_parameter_wrapper * parameters;
} jupx_parameters;


typedef void (*ccHandler)(int8_t);

typedef struct {
  ccHandler handlers[128];
} allCcHandlers;

typedef struct {
  ccHandler cc0;
  ccHandler cc1;
  ccHandler cc2;
  ccHandler cc3;
  ccHandler cc4;
  ccHandler cc5;
  ccHandler cc6;
  ccHandler cc7;
  ccHandler cc8;
  ccHandler cc9;
  ccHandler cc10;
  ccHandler cc11;
  ccHandler cc12;
  ccHandler cc13;
  ccHandler cc14;
  ccHandler cc15;
  ccHandler cc16;
  ccHandler cc17;
  ccHandler cc18;
  ccHandler cc19;
  ccHandler cc20;
  ccHandler cc21;
  ccHandler cc22;
  ccHandler cc23;
  ccHandler cc24;
  ccHandler cc25;
  ccHandler cc26;
  ccHandler cc27;
  ccHandler cc28;
  ccHandler cc29;
  ccHandler cc30;
  ccHandler cc31;
  ccHandler cc32;
  ccHandler cc33;
  ccHandler cc34;
  ccHandler cc35;
  ccHandler cc36;
  ccHandler cc37;
  ccHandler cc38;
  ccHandler cc39;
  ccHandler cc40;
  ccHandler cc41;
  ccHandler cc42;
  ccHandler cc43;
  ccHandler cc44;
  ccHandler cc45;
  ccHandler cc46;
  ccHandler cc47;
  ccHandler cc48;
  ccHandler cc49;
  ccHandler cc50;
  ccHandler cc51;
  ccHandler cc52;
  ccHandler cc53;
  ccHandler cc54;
  ccHandler cc55;
  ccHandler cc56;
  ccHandler cc57;
  ccHandler cc58;
  ccHandler cc59;
  ccHandler cc60;
  ccHandler cc61;
  ccHandler cc62;
  ccHandler cc63;
  ccHandler cc64;
  ccHandler cc65;
  ccHandler cc66;
  ccHandler cc67;
  ccHandler cc68;
  ccHandler cc69;
  ccHandler cc70;
  ccHandler cc71;
  ccHandler cc72;
  ccHandler cc73;
  ccHandler cc74;
  ccHandler cc75;
  ccHandler cc76;
  ccHandler cc77;
  ccHandler cc78;
  ccHandler cc79;
  ccHandler cc80;
  ccHandler cc81;
  ccHandler cc82;
  ccHandler cc83;
  ccHandler cc84;
  ccHandler cc85;
  ccHandler cc86;
  ccHandler cc87;
  ccHandler cc88;
  ccHandler cc89;
  ccHandler cc90;
  ccHandler cc91;
  ccHandler cc92;
  ccHandler cc93;
  ccHandler cc94;
  ccHandler cc95;
  ccHandler cc96;
  ccHandler cc97;
  ccHandler cc98;
  ccHandler cc99;
  ccHandler cc100;
  ccHandler cc101;
  ccHandler cc102;
  ccHandler cc103;
  ccHandler cc104;
  ccHandler cc105;
  ccHandler cc106;
  ccHandler cc107;
  ccHandler cc108;
  ccHandler cc109;
  ccHandler cc110;
  ccHandler cc111;
  ccHandler cc112;
  ccHandler cc113;
  ccHandler cc114;
  ccHandler cc115;
  ccHandler cc116;
  ccHandler cc117;
  ccHandler cc118;
  ccHandler cc119;
  ccHandler cc120;
  ccHandler cc121;
  ccHandler cc122;
  ccHandler cc123;
  ccHandler cc124;
  ccHandler cc125;
  ccHandler cc126;
  ccHandler cc127;
} enumeratedCcHandlers;

typedef union ccHandlers {
  enumeratedCcHandlers enumerated;
  allCcHandlers array;
} ccHandlersView;



typedef struct {
  uint32_t offset;
  uint32_t value;
} readValue;