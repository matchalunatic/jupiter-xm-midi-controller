"""Jupiter XM CC to internal Roland SysEx 
"""

import adafruit_midi
import adafruit_midi.control_change
import adafruit_midi.system_exclusive
import usb_midi
import busio
import board
from time import sleep
from adafruit_midi.control_change import ControlChange
from adafruit_midi.system_exclusive import SystemExclusive
from constants import (
    ROLAND_MANUFACTURER_CODE,
    JUPITER_XM_DEVICE_ID,
    DT1_CODE,
    MIDI_BAUD_RATE,
    MIDI_TIMEOUT,
    TONE_PARTIAL_CUTOFF_FREQ,
    ZC_TONE_PARTIAL_1,
    ZCORE_1,
)

from jupiterlib import RelativeParameter, build_parameter_address

from midiconfig import MidiConfig

from midistate import MidiState


def dbg(*args, **kwargs):
    if MidiConfig.LOG_DEBUG:
        print(*args, **kwargs)


uart = busio.UART(board.TX, board.RX, baudrate=MIDI_BAUD_RATE, timeout=MIDI_TIMEOUT)
dbg("UART is ready", uart.baudrate)


def setup_midi_listeners():
    uart_midi = adafruit_midi.MIDI(
        midi_in=uart, midi_out=uart, debug=MidiConfig.MIDI_DEBUG
    )
    usb_amidi = adafruit_midi.MIDI(
        midi_in=usb_midi.ports[0],
        midi_out=usb_midi.ports[1],
        debug=MidiConfig.MIDI_DEBUG,
    )
    if MidiConfig.UART_MIDI_IN:
        MidiState.midi_receivers.append(uart_midi)
    if MidiConfig.USB_MIDI_IN:
        MidiState.midi_receivers.append(usb_amidi)
    if MidiConfig.UART_MIDI_OUT:
        MidiState.midi_emitters.append(uart_midi)
    if MidiConfig.USB_MIDI_OUT:
        MidiState.midi_emitters.append(usb_amidi)


def handle_midi(message: adafruit_midi.MIDIMessage = None):
    if message is None:
        return
    if message.__class__ == adafruit_midi.control_change.ControlChange:
        channel, cc_code, value = message.channel, message.control, message.value
        for handler in MidiState.parameter_handlers.get((cc_code, channel), []):
            handler.change(value)
    elif message.__class__ == adafruit_midi.system_exclusive.SystemExclusive:
        print([hex(a) for a in message.data])
        if message.data[5] == 0x12:
            addr = tuple(list(message.data[6:10]))
            print("addr", [a for a in addr], MidiState.rq1_handlers)

            for client in MidiState.rq1_handlers.get(addr, []):
                client.read_rq1(message)

# setup_usb_midi()
setup_midi_listeners()
mr = MidiState.midi_receivers
cb = MidiState.callbacks
sleep(1)
rel_test = RelativeParameter(
    110,
    10,
    build_parameter_address(ZCORE_1, ZC_TONE_PARTIAL_1, TONE_PARTIAL_CUTOFF_FREQ),
    0,
    1023,
    4,
    multiplicator=4,
)

while True:
    for m in mr:
        handle_midi(m.receive())
    for c in cb:
        c.send()
