sdrx
====

![C/C++ CI](https://github.com/johanhedin/sdrx/workflows/C/C++%20CI/badge.svg)

`sdrx` is a simple software defined narrow band AM receiver that uses a RTL-SDR
USB dongle as it's hardware part. It's main purpous is to act as a test bench
for different SDR implementation aspects as decimation, filtering, demodulation,
interaction between clock domains, threading and so on. `sdrx` is written in
C++17 and is tested on a x86_64 machine running Fedora 33 and on a Raspberry
Pi 4 Model B 4GiB running Raspberry Pi OS. Audio is played using ALSA.

A RTL-SDR Blog V3 dongle, https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles,
is used for development. The program may be incompatible with other dongles. YMMV.


Build requirements
====
Besides git, gcc, cmake and the standard set of development tools, `sdrx`
depend on the following libraries. They, and their development parts, need to
be installed on your machine in order to build and run `sdrx`:

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


Build
====
The easies way to get `sdrx` is to clone the GitHub repo:

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
`sdrx` is run from the command line and takes a aeronautical channel to listen
to as its sole argument. Besides the channel, some options are available and can
be listed with --help:

    $ cd sdrx
    $ ./sdrx --help
    $ ./sdrx --rf-gain 30 122.455

If you like to use frequency notation in MHz instead of channel notation,
use the `--fq-mode` option:

    $ ./sdrx --fq-mode 118.111034


Output
====
Besides playing audio when the squelch is open, `sdrx` write signal power
measurements to the console while running. How the printouts look change from
time to time and the description below might not always be up-to-date.

A typical output look like this:

    $ ./sdrx -r 45 -l 9 118.105
    ...
    Sql closed. Levels (lo|mid|hi|SNR|imbalance): -22.54|-22.55|-23.44|  0.42|  0.00
    Sql closed. Levels (lo|mid|hi|SNR|imbalance): -22.14|-22.97|-22.21| -0.80|  0.00
    Sql closed. Levels (lo|mid|hi|SNR|imbalance): -21.10|-22.25|-22.81| -0.38|  0.00
    Sql   open. Levels (lo|mid|hi|SNR|imbalance): -21.36| 16.48|-21.72| 38.01|-12.28
    Sql   open. Levels (lo|mid|hi|SNR|imbalance): -22.73| 16.61|-21.57| 38.72|-30.74
    Sql   open. Levels (lo|mid|hi|SNR|imbalance): -21.79| 16.90|-24.18| 39.72|-30.66
    ...

**mid** is the power level in dB for the center band, i.e fc +/- 2.8kHz and
**lo** and **hi** are the power levels for the bands fc - 3.5-4.9kHz and
fc + 3.5-4.9kHz.

The noise level is thus estimated as the power just outside of where the
modulated audo resides. **SNR** is the difference between "signal power" and
"noise power" and is checked against the wanted squelch level to determine if
audio is played or not.

**imbalance** is a measure of how "off" your receiver is compared to the frequency
that the transmitter is using. If it is negative, you are tuned above the
transmitter and if it is positive you are tuned below the transmitter frequency.

The imbalance is determined by calculating the energy balance between "negative"
and "positive" frequencies from the FFT used for the squelch. Since AM is a
symetric modulation around the carrier, the imbalance should be 0 if you are
tuned to the transmitters frequency.
