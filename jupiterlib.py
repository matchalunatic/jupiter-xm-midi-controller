from constants import (
    DEBOUNCE_MIDI_SECONDS,
    JUPITER_X_DEVICE_ID,
    JUPITER_XM_DEVICE_ID,
    DT1_CODE,
    RQ1_CODE,
    ROLAND_MANUFACTURER_CODE,
    SYNTH_ID,
)
from adafruit_midi.system_exclusive import SystemExclusive
from midistate import MidiState
from time import monotonic
from utils import roland_checksum


def integer_to_roland_number(val: int, data_len=4):
    """Convert an integer number to its Roland number representation (one byte per hex figure)
    For example, the value 1024 (hex: 0x3ff) will be represented as follows:
    0x00 0x03 0x0f 0x0f
            ^    ^    ^

    Data is left-padded with 0x00
    """
    data = bytearray(int(a, 16) for a in hex(val)[2:])
    remains = bytearray(0 for _ in range(0, data_len - len(data)))
    remains.extend(data)
    return remains


def roland_number_to_integer(number: tuple):
    total = 0
    numlen = len(number) - 1
    for elem in number:
        total += elem * 16**numlen
        numlen -= 1
    return total


def build_roland_dt1_message(
    address_bytes: tuple, data_bytes: tuple
) -> SystemExclusive:
    """build a roland sysex message based on the address bytes and data bytes, computing the checksum"""
    checksum = roland_checksum(address_bytes=address_bytes, data_bytes=data_bytes)
    ba = bytearray()
    ba.append(SYNTH_ID)
    ba.extend(JUPITER_XM_DEVICE_ID)
    ba.append(DT1_CODE)
    ba.extend(address_bytes)
    ba.extend(data_bytes)
    ba.append(checksum)
    return SystemExclusive(ROLAND_MANUFACTURER_CODE, ba)


def build_roland_rq1_message(address_bytes, data_size):
    data_bytes = integer_to_roland_number(data_size, 4)
    checksum = roland_checksum(address_bytes=address_bytes, data_bytes=data_bytes)
    ba = bytearray()
    ba.append(SYNTH_ID)
    ba.extend(JUPITER_XM_DEVICE_ID)
    ba.append(RQ1_CODE)
    ba.extend(address_bytes)
    ba.extend(data_bytes)
    ba.append(checksum)
    res = SystemExclusive(ROLAND_MANUFACTURER_CODE, ba)
    return res


def validate_rq1_and_get_value(rq1: SystemExclusive):
    if (
        rq1.manufacturer_id != ROLAND_MANUFACTURER_CODE
        and (ord(rq1.manufacturer_id),) != ROLAND_MANUFACTURER_CODE
    ):
        print(
            "not a roland sysex",
            rq1.manufacturer_id,
            "is not",
            ROLAND_MANUFACTURER_CODE,
        )
        return None
    if (
        tuple(rq1.data[1:5]) not in (JUPITER_XM_DEVICE_ID, JUPITER_X_DEVICE_ID)
        and rq1.data[0] != SYNTH_ID
    ):
        print(
            "Not for the right device",
            rq1.data[0],
            tuple(rq1.data[1:5]),
            JUPITER_X_DEVICE_ID,
            JUPITER_XM_DEVICE_ID,
        )
        return None
    address_bytes = tuple(rq1.data[6:10])
    data_bytes = tuple(rq1.data[10:-1])
    checksum = rq1.data[-1]

    our_checksum = roland_checksum(address_bytes, data_bytes)
    if checksum != our_checksum:
        print("Bad checksum", checksum, "!=", our_checksum)
        return None
    return roland_number_to_integer(data_bytes)


def build_set_parameter_message(
    scope_address: tuple,
    subsection: tuple,
    parameter_offset: tuple,
    parameter_value: int,
    parameter_len: int = 4,
) -> SystemExclusive:
    """given a top level scope, subsection offset and parameter offset, set the parameter to the desired value"""
    parameter_address = build_parameter_address(
        scope_address, subsection, parameter_offset
    )
    return build_roland_dt1_message(
        parameter_address, integer_to_roland_number(parameter_value, parameter_len)
    )


def build_parameter_address(
    scope_address: tuple, subsection: tuple, parameter_offset: tuple
):
    return tuple(
        a + b + c for (a, b, c) in zip(scope_address, subsection, parameter_offset)
    )


def get_sysex_data(s: SystemExclusive):
    # use to interpret a DT1 message received
    mfi = s.manufacturer_id
    assert tuple(mfi) == ROLAND_MANUFACTURER_CODE
    d = s.data
    devcode, dev_id, cmd, address, value, checksum = (
        d[0],
        d[1:5],
        d[5],
        d[6:10],
        d[10:-1],
        d[-1],
    )
    assert devcode == SYNTH_ID
    converted_devid = tuple(i for i in dev_id)
    assert converted_devid in (JUPITER_X_DEVICE_ID, JUPITER_XM_DEVICE_ID)
    assert cmd in (0x11, 0x12)
    verified_checksum = roland_checksum(address, value, 0)
    assert verified_checksum == checksum
    addr = int.from_bytes(address, "big")
    val = int.from_bytes(value, "big")
    return addr, val


class RelativeParameter:
    """A relative MIDI parameter"""

    def __init__(
        self,
        cc_code: int,
        midi_channel: int,
        target_address: tuple[int],
        min_value: int,
        max_value: int,
        data_len: int = 4,
        multiplicator: int = 1,
    ):
        self.cc_code = cc_code
        self.midi_channel = midi_channel
        self.target_address = target_address
        self.range = (min_value, max_value)
        self.changed = 0
        self.data_len = data_len
        self.value = 0
        self._monotonic = 0
        self.multiplicator = multiplicator
        self._get_value_from_hardware()
        h = MidiState.parameter_handlers.get((self.cc_code, self.midi_channel), [])
        h.append(self)
        MidiState.parameter_handlers[(self.cc_code, self.midi_channel)] = h
        h2 = MidiState.rq1_handlers.get(self.target_address, [])
        h2.append(self)
        MidiState.rq1_handlers[self.target_address] = h2
        MidiState.callbacks.append(self)

        # self.value = 64 # fixme: fetch from target address using data transfer

    def _get_value_from_hardware(self):

        self.value = 64
        for em in MidiState.midi_emitters:
            em.send(build_roland_rq1_message(self.target_address, self.data_len))

    def _send_value_to_hardware(self):
        if self.changed and monotonic() - self._monotonic > DEBOUNCE_MIDI_SECONDS:
            for em in MidiState.midi_emitters:
                em.send(
                    build_roland_dt1_message(
                        self.target_address,
                        integer_to_roland_number(self.value, self.data_len),
                    )
                )
            self.changed = False
            self._monotonic = 0

    def send(self):
        self._send_value_to_hardware()

    def change(self, val):
        """apply relative midi CC to this parameter"""
        step = val - 64
        self.value += step * self.multiplicator
        self.value = max(self.range[0], min(self.value, self.range[1]))
        if not self.changed:
            self.changed = True
            self._monotonic = monotonic()

    def read_rq1(self, rq1):
        new_val = validate_rq1_and_get_value(rq1)
        if new_val is not None:
            self.value = new_val
