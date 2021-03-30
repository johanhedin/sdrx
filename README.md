sdrx
====

![C/C++ CI](https://github.com/johanhedin/sdrx/workflows/C/C++%20CI/badge.svg)
![CodeQL](https://github.com/johanhedin/sdrx/workflows/CodeQL/badge.svg)

`sdrx` is a software defined narrow band AM airband receiver that uses a
RTL-SDR USB dongle as it's hardware part. It's main purpose is to act as a test
bench for different SDR implementation aspects as translation, down sampling,
filtering, demodulation, interaction between clock domains, threading, audio
processing and so on. `sdrx` is written in C++17 and is tested on a x86_64
machine running Fedora 33 and on a Raspberry Pi 4 Model B 4GiB running
Raspberry Pi OS. Audio is played using ALSA.

A RTL-SDR Blog V3 dongle, https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles,
is used for development. The program may be incompatible with other dongles and
less powerfull Raspberry Pi models. YMMV.


Build requirements
====
Besides git, gcc and the standard set of development tools, `sdrx` depend on
the following libraries. They, and their development parts, need to be
installed on your machine in order to build and run `sdrx`:

 * libusb-1.0
 * librtlsdr
 * libasound
 * libfftw
 * libpopt

On Fedora they can be installed with:

    $ sudo dnf install gcc-c++ cmake git libusbx-devel rtl-sdr-devel alsa-lib-devel \
                       fftw-devel fftw-libs-single popt-devel

On Raspberry Pi OS/Debian/Ubuntu they can be installed with:

    $ sudo apt-get install g++-8 cmake git libusb-1.0-0-dev librtlsdr-dev libasound2-dev \
                           libfftw3-dev libfftw3-single3 libpopt-dev


Download and build
====
The easiest way to get `sdrx` is to clone the GitHub repo:

    $ git clone https://github.com/johanhedin/sdrx.git

and then build with:

    $ cd sdrx
    $ make

To keep up to date with changes, simply run:

    $ cd sdrx
    $ git pull --ff-only
    $ make


Using
====
`sdrx` is run from the command line and takes an aeronautical channel as the
sole argument. Besides the channel, some options are available and can be
listed with --help:

    $ cd sdrx
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

    $ ./sdrs --rf-gain 40 118.105 118.280 118.405 118.505

The more channels you list, the more processing power will be used. For a Pi 4,
the current upper limit is probably at six channels.


Output in single channel mode
====
Besides playing audio when the squelch is open, `sdrx` write signal power
measurements to the console while running.

A typical output look like this:

    $ ./sdrx -r 45 118.105
    ...
    16:44:07 118.105 closed. [lo|mid|hi|SNR|imbalance]: -22.54|-22.55|-23.44|  0.42|  0.00
    16:44:07 118.105 closed. [lo|mid|hi|SNR|imbalance]: -22.14|-22.97|-22.21| -0.80|  0.00
    16:44:07 118.105 closed. [lo|mid|hi|SNR|imbalance]: -21.10|-22.25|-22.81| -0.38|  0.00
    16:44:08 118.105   open. [lo|mid|hi|SNR|imbalance]: -21.36| 16.48|-21.72| 38.01|-12.28
    16:44:08 118.105   open. [lo|mid|hi|SNR|imbalance]: -22.73| 16.61|-21.57| 38.72|-30.74
    16:44:08 118.105   open. [lo|mid|hi|SNR|imbalance]: -21.79| 16.90|-24.18| 39.72|-30.66
    ...

**mid** is the power level in dB for the center band, i.e fc +/- 2.8kHz and
**lo** and **hi** are the power levels for the bands fc - 3.5-4.9kHz and
fc + 3.5-4.9kHz.

The noise floor is estimated as the power just outside of where the modulated
audio resides (i.e. the **lo** and **hi** measurements). **SNR** is the
difference between "signal power" and "noise power" and is checked against the
wanted squelch level to determine if audio is played or not.

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
awareness of what channels that are active.

Output in the terminal looks something like this:

    $ ./sdrx -r 40 118.105 118.280 118.405
    ...
    10:57:00: Level[XX    -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:01: Level[XX    -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:01: Level[XX    -27.5] 118.105[ 0.0] 118.205[ 1.0] 118.280[ 0.0] 118.405[ 0.0]
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
    10:57:05: Level[XX    -27.5] 118.105[ 1.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    10:57:06: Level[XX    -27.5] 118.105[ 1.8] 118.205[ 0.0] 118.280[ 1.2] 118.405[ 0.0]
    10:57:06: Level[XX    -27.5] 118.105[ 0.0] 118.205[ 0.0] 118.280[ 0.0] 118.405[ 0.0]
    ...

Channels with open squelch will have the background color set to yellow. The
number in brackets after every channel is the signal level above the noise
floor (same as SNR for single mode as explained above). Level is IQ sample level
in dB (dBFS) and can be used to see if the receiver is oversteered or not.
