"""Utility for Roland MIDI

Careful: Roland device ID 17 is actually hex 0x10 in sysex... ie it's offset by 1.
"""

import logging

logger = logging.getLogger(__name__)

# def number_to_nibbles(number: int):
#     if number == 0:
#         return [0]
#     number_of_signs = max(1, math.ceil(math.log(number + 1, 16)))
#     res = []
#     for el in range(number_of_signs):
#         o = number >> (el * 4) & 0xf
#         res.append(o)
        
#     res.reverse()
#     return res
def number_to_nibbles(number: int, size=4):
    return [number >> (i * 4) & 0xf for i in range(size - 1, -1, -1)]

def roland_checksum(address_bytes: tuple, data_bytes: tuple, base_checksum_val: int = 0) -> int:
    chk = (128 - ((sum(a for a in address_bytes) + sum(a for a in data_bytes) + base_checksum_val) % 128))
    if chk == 128:
        chk = 0
    return chk

def number_to_bytesarray(number: int, sz=4) -> bytes:
    """Convert an offset integer value to address bytes"""
    return [number >> (i * 8) & 0xff for i in range(sz - 1, -1, -1)]


def offset_to_bytes(offset: int) -> bytes:
    return number_to_bytesarray(offset, 4)

def make_write_sysex_message(offset: int, data: bytes, device_id: int=17, model_id: int=0x65) -> bytes:
    logger.debug("Preparing writing bytes %s to offset %s and to device %s", data, offset, device_id)
    addr = offset_to_bytes(offset)
    ck = roland_checksum(address_bytes=addr, data_bytes=data)
    return bytes((0xf0, 0x41, device_id - 1, 0x00, 0x00, 0x00, model_id, 0x12, *addr, *data, ck, 0xf7))

def make_write_number(offset: int, number: int, size: int, nibbled: bool = True, device_id: int=17, model_id: int=0x65) -> bytes:
    if nibbled:
        data = number_to_nibbles(number, size)
    else:
        data = number_to_bytesarray(number, size)
    return make_write_sysex_message(offset=offset, data=data, device_id=device_id, model_id=model_id)

def make_read_sysex_message(offset: int, size: int, device_id: int=17, model_id: int=0x65) -> bytes:
    logger.debug("Preparing reading %s bytes from offset %s from device %s", size, hex(offset), device_id)
    addr = offset_to_bytes(offset)
    size = number_to_nibbles(size, 4)
    ck = roland_checksum(address_bytes=addr, data_bytes=size)
    return bytes((0xf0, 0x41, device_id - 1, 0x00, 0x00, 0x00, model_id, 0x11, *addr, *size, ck, 0xf7))

def format_for_midiox(b: bytes) -> str:
    return ''.join(['{:02x}'.format(a) for a in list(b)])
if __name__ == '__main__':
    from random import randint
    from  jupiterx_addresses import JupiterXAddresses
    logging.basicConfig()
    new_lforate = randint(10, 1020)
    se = make_write_number(JupiterXAddresses.TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_LFO_RATE_OFFSET, new_lforate, JupiterXAddresses.TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_LFO_RATE_SIZE, device_id=17)
    re = make_read_sysex_message(JupiterXAddresses.TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_LFO_RATE_OFFSET, JupiterXAddresses.TEMPORARY_TONE_ANALOG_SYNTH_MODEL_0_MDLSYN0_LFO_RATE_SIZE)    
    print("new lfo rate:", new_lforate)
    print(format_for_midiox(se))
    print(format_for_midiox(re))
