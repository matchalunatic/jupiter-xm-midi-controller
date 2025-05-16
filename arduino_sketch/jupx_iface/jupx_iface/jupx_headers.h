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


typedef void (*ccHandlerType)(int8_t);

typedef struct {
  ccHandlerType handlers[128];
} allCcHandlers;

typedef struct {
  ccHandlerType cc0;
  ccHandlerType cc1;
  ccHandlerType cc2;
  ccHandlerType cc3;
  ccHandlerType cc4;
  ccHandlerType cc5;
  ccHandlerType cc6;
  ccHandlerType cc7;
  ccHandlerType cc8;
  ccHandlerType cc9;
  ccHandlerType cc10;
  ccHandlerType cc11;
  ccHandlerType cc12;
  ccHandlerType cc13;
  ccHandlerType cc14;
  ccHandlerType cc15;
  ccHandlerType cc16;
  ccHandlerType cc17;
  ccHandlerType cc18;
  ccHandlerType cc19;
  ccHandlerType cc20;
  ccHandlerType cc21;
  ccHandlerType cc22;
  ccHandlerType cc23;
  ccHandlerType cc24;
  ccHandlerType cc25;
  ccHandlerType cc26;
  ccHandlerType cc27;
  ccHandlerType cc28;
  ccHandlerType cc29;
  ccHandlerType cc30;
  ccHandlerType cc31;
  ccHandlerType cc32;
  ccHandlerType cc33;
  ccHandlerType cc34;
  ccHandlerType cc35;
  ccHandlerType cc36;
  ccHandlerType cc37;
  ccHandlerType cc38;
  ccHandlerType cc39;
  ccHandlerType cc40;
  ccHandlerType cc41;
  ccHandlerType cc42;
  ccHandlerType cc43;
  ccHandlerType cc44;
  ccHandlerType cc45;
  ccHandlerType cc46;
  ccHandlerType cc47;
  ccHandlerType cc48;
  ccHandlerType cc49;
  ccHandlerType cc50;
  ccHandlerType cc51;
  ccHandlerType cc52;
  ccHandlerType cc53;
  ccHandlerType cc54;
  ccHandlerType cc55;
  ccHandlerType cc56;
  ccHandlerType cc57;
  ccHandlerType cc58;
  ccHandlerType cc59;
  ccHandlerType cc60;
  ccHandlerType cc61;
  ccHandlerType cc62;
  ccHandlerType cc63;
  ccHandlerType cc64;
  ccHandlerType cc65;
  ccHandlerType cc66;
  ccHandlerType cc67;
  ccHandlerType cc68;
  ccHandlerType cc69;
  ccHandlerType cc70;
  ccHandlerType cc71;
  ccHandlerType cc72;
  ccHandlerType cc73;
  ccHandlerType cc74;
  ccHandlerType cc75;
  ccHandlerType cc76;
  ccHandlerType cc77;
  ccHandlerType cc78;
  ccHandlerType cc79;
  ccHandlerType cc80;
  ccHandlerType cc81;
  ccHandlerType cc82;
  ccHandlerType cc83;
  ccHandlerType cc84;
  ccHandlerType cc85;
  ccHandlerType cc86;
  ccHandlerType cc87;
  ccHandlerType cc88;
  ccHandlerType cc89;
  ccHandlerType cc90;
  ccHandlerType cc91;
  ccHandlerType cc92;
  ccHandlerType cc93;
  ccHandlerType cc94;
  ccHandlerType cc95;
  ccHandlerType cc96;
  ccHandlerType cc97;
  ccHandlerType cc98;
  ccHandlerType cc99;
  ccHandlerType cc100;
  ccHandlerType cc101;
  ccHandlerType cc102;
  ccHandlerType cc103;
  ccHandlerType cc104;
  ccHandlerType cc105;
  ccHandlerType cc106;
  ccHandlerType cc107;
  ccHandlerType cc108;
  ccHandlerType cc109;
  ccHandlerType cc110;
  ccHandlerType cc111;
  ccHandlerType cc112;
  ccHandlerType cc113;
  ccHandlerType cc114;
  ccHandlerType cc115;
  ccHandlerType cc116;
  ccHandlerType cc117;
  ccHandlerType cc118;
  ccHandlerType cc119;
  ccHandlerType cc120;
  ccHandlerType cc121;
  ccHandlerType cc122;
  ccHandlerType cc123;
  ccHandlerType cc124;
  ccHandlerType cc125;
  ccHandlerType cc126;
  ccHandlerType cc127;
} enumeratedCcHandlers;

typedef union ccHandlers {
  enumeratedCcHandlers enumerated;
  allCcHandlers array;
} ccHandlersView;



typedef struct {
  uint32_t offset;
  uint32_t value;
} readValue;