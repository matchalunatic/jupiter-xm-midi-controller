# Jupiter XM MIDI controller

The Jupiter XM is very nice but it lacks a bunch of interface elements.

It's so complex that Roland doesn't provide a MIDI implementation based on CC codes to do stuff. Understandable.

This piece of software and hardware (based on Adafruit Feather with the MIDI wing) aims to fix that by providing decent MIDI mapping to internal parameters at least for performance needs.

It works quite simply:

- listen to MIDI CC events on the input port
- translate them to sysex instructions and throw these on the output port

Configuration is done by mapping CC codes to specific parameters.
Some parameters multi-aliases exist: for example, you can send at the same time the SysEx code for Analog Modeling Cutoff on part 1 and for the ZCore cutoff on part 1 so your cutoff will change no matter the scene type.

I also (plan to) support response curves, so that response may be linear (default), logarithmic, exponential, or otherwise.

And finally, I would like to support relative controls for infinite rotative knobs such as the ones featured on the Keystep Pro: these map very well to the 1024-steps parameters of the X and XM.
