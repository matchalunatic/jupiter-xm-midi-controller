import rtmidi
import rtmidi.midiutil

def build_rtmidi_inout() -> tuple[rtmidi.MidiIn, rtmidi.MidiOut]:
    available_in = rtmidi.MidiIn().get_ports()
    available_out = rtmidi.MidiOut().get_ports()
    (jupi_in,) = (
        a
        for a in available_in
        if a.startswith("JUPITER-X")
        and "DAW CTRL" not in a
        and "Jupiter X Controller" not in a
    )
    (jupi_out,) = (
        a
        for a in available_out
        if a.startswith("JUPITER-X")
        and "DAW CTRL" not in a
        and "Jupiter X Controller" not in a
    )
    return rtmidi.midiutil.open_midiinput(jupi_in), rtmidi.midiutil.open_midioutput(
        jupi_out
    )
