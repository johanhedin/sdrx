sdrx
====

[![Build](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml)
[![CodeQL](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml)

`sdrx` is a software defined narrow band multi channel AM airband receiver that
uses a RTL-SDR USB dongle as it's hardware part. It's main purpose is for
experimenting with different SDR implementation aspects as translation, down
sampling, filtering, demodulation, interaction between clock domains, threading,
audio processing and so on. `sdrx` is written in C++17 and is tested on a x86_64
machine running Fedora 33 and on a Raspberry Pi 4 Model B 4GiB running
Raspberry Pi OS. Audio is played using ALSA.

A RTL-SDR Blog V3 dongle, https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles,
is used for development. The program may be incompatible with other RTL dongles
and less powerfull Raspberry Pi models. YMMV.

Support for Airspy R2 and Airspy Mini is worked on.


Build requirements
====
Besides git, gcc, cmake and the standard set of development tools, `sdrx` depend
on the following libraries. They, and their development parts, need to be
installed on your machine in order to build and run `sdrx`:

 * libpopt
 * libusb-1.0
 * librtlsdr
 * libairspy
 * libasound
 * libfftw

On Fedora they can be installed with:

    $ sudo dnf install git gcc-c++ cmake popt-devel libusbx-devel rtl-sdr-devel alsa-lib-devel \
                       fftw-devel fftw-libs-single airspyone_host

On Raspberry Pi OS/Debian/Ubuntu they can be installed with:

    $ sudo apt-get install git g++-8 cmake libpopt-dev libusb-1.0-0-dev librtlsdr-dev libasound2-dev \
                           libfftw3-dev libfftw3-single3 libairspy0


Download and build
====
The easiest way to get `sdrx` is to clone the GitHub repo. Since `sdrx` depend
on the latest libairspy and librtlsdr, these projects are included as
submodules at the moment:

    $ git clone --recurse-submodules https://github.com/johanhedin/sdrx.git
    $ cd sdrx

and then build with cmake:

    $ mkdir build
    $ cd build
    $ cmake ../
    $ make

To keep up to date with changes, simply run:

    $ cd sdrx
    $ git pull --ff-only
    $ git submodule update --init --recursive
    $ cd build
    $ cmake ../
    $ make

The arguments to `git submodule update` will assure that newly added submodules
are checked out properly. For existing submodules, they are a no-op. If the
build fails after a update, try to remove all files inside the build directory
and start over:

    $ cd build
    $ rm -rf *
    $ cmake ../
    $ make


Using
====
`sdrx` is run from the command line and takes an aeronautical channel as the
sole argument. Besides the channel, some options are available and can be
listed with --help:

    $ cd sdrx/build
    $ ./sdrx --help
    $ ./sdrx --rf-gain 30 122.455

The defaults for audio gain and squelsh level should be good as is. RF gain
can be adjusted according to the local radio environment. Also note that `sdrx`
use quite narrow filters so if your dongle does not have a TCXO, take your
time to find out the proper frequency correction and supply that with the
--fq-corr option.

Support for multiple channels are available as well. Just list the channels as
arguments (due to the fixed sampling frequency used, they must fit inside a
1MHz band):

    $ ./sdrx --rf-gain 40 118.105 118.280 118.405 118.505

The more channels you list, the more processing power will be used. For a Pi 4,
the current upper limit is probably at six channels.


Output in single channel mode
====
Besides playing audio when the squelch is open, `sdrx` write signal power
measurements to the console while running at approximately three times a
second.

A typical output look like this:

    $ ./sdrx -r 45 118.105
    ...
    10:57:00: Level[XX    -37.2] 118.105[ 0.0|-22.5|-22.6|-23.4|  0.00] (SNR|low|mid|hig|imbalance)
    10:57:01: Level[XX    -39.6] 118.105[ 0.0|-22.1|-22.9|-22.2|  0.00] (SNR|low|mid|hig|imbalance)
    10:57:01: Level[XX    -39.5] 118.105[ 0.0|-21.1|-22.3|-22.8|  0.00] (SNR|low|mid|hig|imbalance)
    10:57:01: Level[XXXX  -27.5] 118.105[38.0|-21.4| 16.5|-21.7|-12.28] (SNR|low|mid|hig|imbalance)
    10:57:02: Level[XXXX  -27.5] 118.105[38.7|-22.7| 16.6|-21.6|-30.74] (SNR|low|mid|hig|imbalance)
    10:57:02: Level[XXXX  -27.5] 118.105[39.7|-21.8| 16.9|-24.2|-30.66] (SNR|low|mid|hig|imbalance)
    ...

Open squelch is indicated by the channel name getting a yellow background.
**Level** is IQ sample level in dB (dBFS) and can be used to see if the receiver is
oversteered or not.

**mid** is the power level in dB for the center band, i.e fc +/- 2.8kHz and
**low** and **hig** are the power levels for the bands fc - 3.5-4.9kHz and
fc + 3.5-4.9kHz.

The noise floor is estimated as the power just outside of where the modulated
audio resides (i.e. the **low** and **hig** measurements). **SNR** is the
difference between "signal power" and "noise power" and is checked against the
desired squelch level to determine if audio is played or not.

**imbalance** is a measure of how "off" the receiver is compared to the frequency
that the transmitter is using. If it is negative, you are tuned above the
transmitter and if it is positive you are tuned below the transmitter frequency.

The imbalance is determined by calculating the energy balance between "negative"
and "positive" frequencies from the FFT used for the squelch. Since AM is a
symmetric modulation around the carrier, the imbalance should be 0 if you are
tuned to the transmitters frequency.


Output in multi channel mode
====
In multi channel mode, audio from the different channels are distributed between
the left and right speaker in a audio panorama. This will create the sense of a
set of speakers, one per channel, in front of the listener.

Together with the visual presentation in the terminal, this will increase the
awareness of what channels that are active. The channels will be placed in the
panorma based on their order on the command line.

Output in the terminal looks about the same as for signle channel but only the
SNR is shown for the channels:

    $ ./sdrx -r 40 118.105 118.280 118.405
    ...
    10:57:00: Level[XX    -39.6] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:01: Level[XX    -39.6] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:01: Level[XX    -39.6] 118.105[ 0.0] 118.205[ 1.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:01: Level[XXXX  -27.5] 118.105[ 1.3] 118.205[ 0.0] 118.280[27.0] 118.405[ 0.0]
    10:57:02: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.5] 118.405[ 0.0]
    10:57:02: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.9] 118.405[ 0.0]
    10:57:02: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[29.0] 118.405[ 0.0]
    10:57:03: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[29.4] 118.405[ 0.0]
    10:57:03: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.2] 118.405[ 0.0]
    10:57:03: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.4] 118.405[ 0.0]
    10:57:04: Level[XXXX  -27.5] 118.105[ 1.1] 118.205[ 0.0] 118.280[29.8] 118.405[ 0.0]
    10:57:04: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.9] 118.405[ 0.0]
    10:57:04: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.5] 118.405[ 0.0]
    10:57:05: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[27.9] 118.405[ 0.0]
    10:57:05: Level[XXXX  -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[28.8] 118.405[ 0.0]
    10:57:05: Level[XX    -39.6] 118.105[ 1.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:06: Level[XX    -39.6] 118.105[ 1.8] 118.205[ 0.0] 118.280[ 1.2] 118.405[ 0.0]
    10:57:06: Level[XX    -39.6] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    ...
