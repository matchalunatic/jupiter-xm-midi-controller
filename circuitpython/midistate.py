import adafruit_midi


class MidiState:
    midi_receivers: list[adafruit_midi.MIDI] = []
    midi_emitters: list[adafruit_midi.MIDI] = []
    parameter_handlers = {}
    rq1_handlers = {}
    callbacks: list["sendable"] = []
