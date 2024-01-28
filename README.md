sdrx
====

[![Build](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml)
[![CodeQL](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml)

`sdrx` is a software defined multichannel narrowband AM airband receiver that
uses R820T(2)/R860 based RTL-SDR or Airspy Mini/R2 dongles as it's hardware
part. `sdrx` is written in C++ (C++-20) and regularly tested on a x86_64 machine
running Fedora 39, on a Raspberry Pi 4 Model B 4GiB and on a Raspberry Pi Zero 2 W
running the latest Raspberry Pi OS (mostly the 64-bit version). Audio handling
is done with ALSA.

Dongles used for the development are: [RTL-SDR Blog V3](https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles),
[Nooelec NESDR SMArt v5](https://www.nooelec.com/store/sdr/sdr-receivers/smart/nesdr-smart-sdr.html),
[Airspy Mini](https://airspy.com/airspy-mini) and [Airspy R2](https://airspy.com/airspy-r2).
The program only support R820T(2)/R860 based devices and may be incompatible
with other RTL dongles and less powerfull Raspberry Pi models. YMMV.

The new R828D tuner based [RTL-SDR Blog V4](https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles)
dongle is not tested with `sdrx` and might, or might not, work.

`sdrx` is a text console program intended to be run from a terminal. Basic Linux
understanding is expected from the user, especially with respect to how to
run programs from the command line.

The channelization is currently done with a translate, filter and downsampling
approach in the time domain. This is simple, but not the most efficient way
when listening to many simultaneous channels. Other methods are being considered
in the future.


Features
----
Below is a list of the current main features of `sdrx`:

 * Multichannel. Listen to multiple channels simultaneously
 * Airspy R2/Mini and R820T(2)/R860 based RTL dongle support
 * Up to 8MHz of RF bandwidth (Airspy R2 or Mini in 10 MS/s IQ)
 * Per-channel auto SNR based squelsh
 * Auto restart of dongles if they are unplugged and then plugged in again
 * Audio panorama. Places your channels in a virtual left-right audio panorama for increased awareness
 * Proper airband channel designators instead of frequency. Supports both the 8.33kHz and the 25kHz naming schemes
 * Full individual control over the R820T(2)/R860 gain stages (LNA, Mixer and VGA)
 * Channel based waterfall (-ish) in the terminal


Download and build
----
`sdrx` is only available in source code form. It is easy to build using the
instructions on the [build](doc/BUILD.md) page.


Using
----
Instruction for how to run and use `sdrx` can be found on the [usage](doc/USING.md)
page.
