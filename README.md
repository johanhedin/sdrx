sdrx
====

[![Build](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml)
[![CodeQL](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml)

`sdrx` is a software defined narrow band multi channel AM airband receiver that
uses R820T(2)/R860 based RTL-SDR or a Airspy Mini/R2 dongles as it's hardware
part. It's also a program for experimenting with different SDR implementation
aspects such as translation, downsampling, filtering, channelization,
demodulation, interaction between clock domains, threading and audio processing.
`sdrx` is written in C++17 and is tested on a x86_64 machine running Fedora 35,
on a Raspberry Pi 4 Model B 4GiB and on a Raspberry Pi Zero 2 W running Raspberry
Pi OS. Audio is played using ALSA.

The dongles used during development are; [RTL-SDR Blog V3](https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles),
[Nooelec NESDR SMArt v4](https://www.nooelec.com/store/sdr/sdr-receivers/smart/nesdr-smart-sdr.html),
[Airspy Mini](https://airspy.com/airspy-mini) and [Airspy R2](https://airspy.com/airspy-r2).
The program only support R820T(2)/R860 based devices and may be incompatible
with other RTL dongles and less powerfull Raspberry Pi models. YMMV.

`sdrx` is a text console program intended to be run from a terminal. Basic Linux
understanding is expecetd from the user, especially with respect to how to
run programs from the command line.


Download and build
====
`sdrx` is only available in source code form. It is easy to build using the
instructions on the [build](doc/BUILD.md) page.


Using
====
Instructions for how to use `sdrx` can be found [here](doc/USING.md).
