sdrx
====
`sdrx` is a simple software defined narrow band AM receiver that uses a RTL-SDR
USB dongle as it's hardware part. It's main purpous is to act as a
experimentation bench for different SDR implementation aspects as decimation,
filtering, demodulation, interaction between clock domains, efficent threading
and so on. `sdrx` is written in C++ and is continuously tested on x86_64 with
Fedora 33 and on a Raspberry Pi 4 Model B 4GiB running Raspberry Pi OS.


Requirements
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

    $ sudo dnf install gcc-c++ cmake git libusbx-devel rtl-sdr-devel alsa-lib-devel fftw-devel fftw-libs-single popt-devel


Checking out and building
====
The easies way to get `sdrx`is to clone the GitHub repo:

    $ git clone https://github.com/johanhedin/sdrx.git

and then build with:

    $ cd sdrx
    $ make

To keep up to date with upstream, simply run:

    $ cd sdrx
    $ git pull --ff-only
    $ make


Running
====
`sdrx`is run from the command line and take the frequency to listen on as
its sole argument. Besides the frequency, some other options are available
and can be listed with --help:

    $ sdrx --help
    $ sdrx --rf-gain 30 122.450

The audio is played using ALSA.
