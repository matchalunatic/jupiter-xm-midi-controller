import logging

logger = logging.getLogger(__name__)
logger.level = logging.WARNING


def lhex(n: int, l: int = 8):
    return "0x" + hex(n)[2:].zfill(l)  #'0x002a'


def bhex(b: bytes, l: int = 8):
    n = int.from_bytes(b, "big")
    return lhex(n, l)


def nlhex(n: int, l: int = 8, g: int = 4):
    s = hex(n)[2:].zfill(l)
    o = ""
    i = 0
    while i < len(s):
        o += f"{s[i:i+g]} "
        i += g
    # o += f"{s[i-g:i]}."
    return o


class midi7bitrange:
    def __init__(self, start: int, stop: int = None, step: int = None):
        if stop is None:
            stop = start
            start = 0
        if step is None:
            step = 1
        assert int(step) == step and start == int(start) and stop == int(stop)
        assert step != 0
        self.step = step
        self.start = start
        self.stop = stop
        self.positive = step > 0

    def all_numbers(self):
        n = self.start
        0b0010100_0000000_0000000


class floatrange:
    def __init__(self, start: float, stop: float = None, step: float = None):
        if stop is None:
            stop = start
            start = 0.0
        if step is None:
            step = 1.0
        assert step > 0
        self.step = step
        self.start = start
        self.stop = stop

    def __len__(self):
        return int((self.stop - self.start) / self.step)

    def __iter__(self):
        n = self.start
        while n < self.stop:
            yield n
            n += self.step

    def __repr__(self):
        return f"floatrange(start={self.start}, stop={self.stop}, step={self.step})"


def roland_checksum(
    address_bytes: tuple, data_bytes: tuple, base_checksum_val: int = 0
) -> int:
    chk = 128 - (
        (sum(a for a in address_bytes) + sum(a for a in data_bytes) + base_checksum_val)
        % 128
    )
    if chk == 128:
        chk = 0
    return chk


def rollover_arithmetic(address: int, offset: int, max_length: int = 4, bits: int = 7):
    # help do unholy arithmetic of n-bit with n < 8
    # like MIDI expects us to do with 7 bit
    address_as_bytes = address.to_bytes(max_length, "little")
    offset_as_bytes = offset.to_bytes(max_length, "little")
    out = []
    carry = 0
    for n, m in zip(address_as_bytes, offset_as_bytes):
        s = n + m + carry
        carry = 0
        if s > (1 << bits) - 1:
            s = s - (1 << bits)
            carry = 1
        out.append(s)
    return int.from_bytes(out, "little")


def roland_address_arithmetic(address: int, stride: int):
    # take an address in hex form
    # if address + stride has any bit over 7f
    # subtract 0x80 from it
    # carry the more significant bit to + 1
    return rollover_arithmetic(address, stride, 4, 7)


class roland_address_strider:
    """Given a first offset, last offset, and stride, provide the succession of addresses"""

    def __init__(
        self, first_offset: int, end_offset: int, stride: int, friendly_name: str = ""
    ):
        self.first_offset = first_offset
        self.end_offset = end_offset
        if stride == 0:
            raise ValueError("roland_address_strider: stride can never be 0")
        self.stride = stride
        self.friendly_name = friendly_name
        self._aslist_ = None

    def _aslist(self):
        if self._aslist_ is None:
            self._aslist_ = list(iter(self))
        return self._aslist_

    def __iter__(self):
        a = self.first_offset
        while a <= self.end_offset:
            yield a
            a = roland_address_arithmetic(a, self.stride)

    def __len__(self):
        return len(self._aslist())

    def __getitem__(self, key):
        try:
            return self._aslist()[key]
        except IndexError:
            logger.error(
                "strider %s: Try to access stride %s but maximum is %s",
                self.friendly_name,
                key,
                len(self),
            )
            raise

    def __repr__(self):
        return f"roland_address_strider<{self.friendly_name}>(first_offset={lhex(self.first_offset)}, end_offset={lhex(self.end_offset)}, stride={lhex(self.stride)}) [L={len(self)}]"
