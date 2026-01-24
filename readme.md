# Portable Video Adapter

An RP2350-based video converter for the Macintosh Portable and Outbound 125, using PicoDVI for video output.

> [!CAUTION]
> Never use regular VGA cables to connect anything the Portable or Outbound video output without thoroughly testing them first.
Most VGA cables will have pins shorted together or to the shield. All 15 pins must be straight-through and no pins connected to the shield.

## Adapter Boards

These "hat" adapter boards combined with the Waveshare RP2350-PiZero board are a quick way to get video output from your Portable or Outbound.

> [!CAUTION]
> Never use the Portable adapter board with the Outbound or vice versa.

See [hardware/portable-hat](hardware/portable-hat) for the Macintosh Portable hat schematics.  
See [hardware/outbound-hat](hardware/outbound-hat) for the Outbound 125 hat schematics.

For the board files, go to [Releases](https://github.com/rezafouladian/PortableVideoAdapter/releases).

## Video Output

Currently, the video output is set to 640x400 @ 70 Hz.

## License

Hardware is licensed under the CERN Open Hardware License Version 2 - Permissive.  
Software is licensed under the MIT License.

PicoDVI and pico-sdk are licensed under the BSD 3-Clause License.