"""Jupiter XM CC to internal Roland SysEx 
"""
import adafruit_midi
import usb_midi

from adafruit_midi.control_change import ControlChange
from adafruit_midi.system_exclusive import SystemExclusive
from constants import ROLAND_MANUFACTURER_CODE, JUPITER_XM_DEVICE_ID, DT1_CODE

def integer_to_roland_number(val: int, data_len=4):
    """Convert an integer number to its Roland number representation (one byte per hex figure)
    For example, the value 1024 (hex: 0x3ff) will be represented as follows:
    0x00 0x03 0x0f 0x0f
            ^    ^    ^
    
    Data is left-padded with 0x00
    """
    data = bytearray(int(a, base=16) for a in hex(val))
    while len(data) < data_len:
        data.insert(0, 0)
    return data

def build_roland_sysex_message(address_bytes: tuple, data_bytes: tuple):
    """build a roland sysex message based on the address bytes and data bytes, computing the checksum"""
    checksum = 128 - (((sum(a) for a in address_bytes) + (sum(a) for a in data_bytes)) % 128)
    ba = bytearray(JUPITER_XM_DEVICE_ID)
    ba.extend(DT1_CODE)
    ba.extend(address_bytes)
    ba.extend(data_bytes)
    ba.append(checksum)
    return SystemExclusive(ROLAND_MANUFACTURER_CODE, ba)

def build_set_parameter_message(scope_address: tuple, subsection: tuple, parameter_offset: tuple, parameter_value: int, parameter_len: int = 4):
    """given a top level scope, subsection offset and parameter offset, set the parameter to the desired value"""
    parameter_address = tuple(a + b + c for (a, b, c) in zip(scope_address, subsection, parameter_offset))
    return build_roland_sysex_message(parameter_address, integer_to_roland_number(parameter_value, parameter_len)) 
