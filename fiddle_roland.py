#!/usr/bin/env python

from jupiterlib import roland_checksum, build_roland_dt1_message, build_roland_rq1_message, get_sysex_data, validate_rq1_and_get_value, build_parameter_address
from utils import lhex, bhex, roland_address_arithmetic
from midi_utils import build_rtmidi_inout
from adafruit_midi.system_exclusive import SystemExclusive
from constants import ZCORE_1, ZC_TONE_PARTIAL_1, TONE_PARTIAL_CUTOFF_FREQ
from time import sleep
(_midi_in, _midi_out) = build_rtmidi_inout()
midi_in = _midi_in[0]
print("IN port name: ", _midi_in[1])
midi_out = _midi_out[0]
print("OUT port name: ", _midi_out[1])

sysex_state = {
    'strings': []
}


def show_midi_value(mesg_tuple, data):
    midibytes, delta_t = mesg_tuple

    if midibytes[0] == 0xf0 and midibytes[-1] == 0xf7:

        bb = bytes(midibytes)
        try:
            addr, val = get_sysex_data(SystemExclusive.from_bytes(bb))
            # print(f"Memory {lhex(addr, 8)} = {lhex(val, 8)}")
            
            to_bytes = val.to_bytes(64, 'big')
            s = ''
            for b in to_bytes:
                if 32 <= b <= 127:
                    s += chr(b)

            if len(s) > 8:
                # print("Dumped string:", s)
                print(f"String {s} found at\t{lhex(addr)}")
                sysex_state['strings'].append(s)
        except AssertionError:
            print("Unhandled SysEx data")
        
midi_in.set_callback(show_midi_value)
midi_in.ignore_types(sysex=False)
def repl():
    while True:
        addr = input("Give sysex address in hex form without 0x -> ")
        size = input("Give input size in decimal                -> ")
        if not addr:
            addr_to_hexbytes = build_parameter_address(ZCORE_1, ZC_TONE_PARTIAL_1, TONE_PARTIAL_CUTOFF_FREQ)
            addr_to_hexbytes = tuple(a + b for a, b in  zip(addr_to_hexbytes, [0, 0, 0, 0]))
            print("Default addr", addr_to_hexbytes)
        else:
            addr_to_hexbytes = int(addr, base=16).to_bytes(4, "big")
        if not size:
            size = "4"
            print("Default size", size)
        size = int(size, base=10)    
        m = build_roland_rq1_message(addr_to_hexbytes, size)

        r = validate_rq1_and_get_value(m)
        print('raw hex ' + ' '.join(lhex(a, 2)[2:] for a in bytes(m)) + ' dec')

        out = midi_out.send_message(bytes(m))
        print(out)

def mainloop():
    #start = 0x40000000
    #start = 0x407d0000
    # block at 0x0x407d0000 = Night Race
    start = 0x407d0000
    #start = 0x41000000
    stride = 0x50000
    end = 0x497b0000
    for idx, elem in enumerate(range(start, end + 1, stride)):
        elem_addr = elem.to_bytes(4, 'big')
        length = 16
        r = build_roland_rq1_message(elem_addr, length)
        input(f'Press enter to send request for {bhex(elem_addr)}\n')
        midi_out.send_message(bytes(r))

def mainloop_b():
    # JP Layer /SL1 is 3-01 and is found at 0x41200000
    start_search = 0x40_00_00_00
    striding = 0x50000
    data_len = 16
    len_of_strings = len(sysex_state['strings'])
    skip_start = 0
    start_search += skip_start * striding
    total_len = len(range(start_search, start_search + 0x1_00_00_00, striding))
    print("Total len", total_len)
    for idx, elem in enumerate(range(start_search, start_search + 0x1_00_00_00, striding)):
        r = build_roland_rq1_message(elem.to_bytes(4, 'big'), data_len)
        midi_out.send_message(bytes(r))
        sleep(0.2)
        
        if len(sysex_state['strings']) > len_of_strings:
            print("found a new string", sysex_state['strings'][len_of_strings], 'at ', lhex(elem, 8), '\t', bin(elem), f'({idx})')
            len_of_strings = len(sysex_state['strings'])
        if (idx % 10) == 0:
            print("idx:", idx, bhex(elem.to_bytes(4, 'big')))

def mainloop_c():
    start_search = 0x41_02_00_00
    last_address = 0x49_7b_00_00
    #start_search = 0x49_7b_00_00
    #last_address = 0x40_00_00_00
    stride = 0x5_00_00
    # stride = -0x5_00_00
    data_len = 15
    a = start_search
    found = 0
    while a < last_address:
        addr_as_bytes = a.to_bytes(4, 'big')
        r = build_roland_rq1_message(addr_as_bytes, data_len)
        midi_out.send_message(bytes(r))
        sleep(0.2)
        a += stride
        rev = list(reversed(addr_as_bytes))
        new_a = []
        overflow = 0
        overflowed = False
        for idx, el in enumerate(rev):
            el += overflow
            seven_bit_val = el % 127
            new_a.append(seven_bit_val)
            overflow = el // 128
            overflowed = overflowed or overflow > 0
            if overflow:
                print(f"bit {idx + 1}/{len(rev)} overflows: {hex(el)}. Truncate to {hex(seven_bit_val)} and set overflow {overflow}")
        if overflow > 0:
            print("hu ho bad overflow situation")
        
        new_a = list(reversed(new_a))
        a = int.from_bytes(new_a, 'big')
        a += stride
        if overflowed:
            print("let's see what we have")
            print('\n\t'.join(sysex_state['strings']))
            print(f"Found {len(sysex_state['strings'])} entries")
            sysex_state['strings'] = []
            print("new starting point is now", lhex(a))
            return
        
def mainloop_d():
    start_address = 0x40_00_00_00
    stride = 0x5_00_00
    end_address = 0x49_7b_00_00
    data_len = 15
    a = start_address
    while a <= end_address:
        addr_as_bytes = a.to_bytes(4, 'big')
        r = build_roland_rq1_message(addr_as_bytes, data_len)
        print(bhex(bytes(r)))
        midi_out.send_message(bytes(r))
        sleep(0.05)
        a = roland_address_arithmetic(a, stride)
        # print(f"New address: {lhex(a)}")
    print(f"Found {len(sysex_state['strings'])} strings")
if __name__ == '__main__':
    import sys
    if len(sys.argv) > 1 and sys.argv[1] == 'repl':
        repl()
    else:
        mainloop_d()