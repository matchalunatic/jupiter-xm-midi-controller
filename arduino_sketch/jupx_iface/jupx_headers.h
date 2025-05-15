typedef struct {
  uint32_t offset;
  byte cc_number;
  uint8_t *current_value;
  uint8_t range_min;
  uint8_t range_max;

} ubyte_jupx_parameter;

typedef struct {
  uint32_t offset;
  byte cc_number;
  uint16_t *current_value;
  uint16_t range_min;
  uint16_t range_max;
} ushort_jupx_parameter;

typedef struct {
  uint32_t offset;
  byte cc_number;
  uint32_t *current_value;
  uint32_t range_min;
  uint32_t range_max;
} uint_jupx_parameter;
