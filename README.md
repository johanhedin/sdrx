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

If you like to use frequency notation instead of channel notation, use the
`--fq-mode` option:

    $ ./sdrx --fq-mode 118.111034
