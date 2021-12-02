sdrx
====

[![Build](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/c-cpp.yml)
[![CodeQL](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml/badge.svg)](https://github.com/johanhedin/sdrx/actions/workflows/codeql-analysis.yml)

`sdrx` is a software defined narrow band multi channel AM airband receiver that
uses a R820T(2)/R860 based RTL-SDR or a Airspy Mini/R2 dongle as it's hardware
part. It's also a program for experimenting with different SDR implementation
aspects such as translation, downsampling, filtering, demodulation, interaction
between clock domains, threading, audio processing and so on. `sdrx` is written
in C++17 and is tested on a x86_64 machine running Fedora 35 and on a Raspberry
Pi 4 Model B 4GiB running Raspberry Pi OS. Audio is played using ALSA.

The channelization is done with a translate, filter and downsampling approach in
one single thread. This is simple, but not the most effective way when listening
to many simultaneous channels. Other, more effective methods are being considered
in the future.

For development, [RTL-SDR Blog V3](https://www.rtl-sdr.com/buy-rtl-sdr-dvb-t-dongles),
[Airspy Mini](https://airspy.com/airspy-mini) and [Airspy R2](https://airspy.com/airspy-r2)
dongles are used. The program only support R820T(2)/R860 based dongles and may be
incompatible with other RTL dongles and less powerfull Raspberry Pi models. YMMV.

`sdrx` is only available in source code form. It is easily build using the
instructions below, but some basic Linux understanding and familiarity with a
terminal/bash is expected from the user. Especially with respect to how to
run programs from the command line.


Build requirements
====
Besides git, gcc, cmake and the standard set of development tools, `sdrx` depend
on the following libraries. They, and their development parts, need to be
installed on your machine in order to build and run `sdrx`:

 * libpopt
 * libsigc++20
 * libusb-1.0
 * libfftw
 * libasound
 * librtlsdr
 * libairspy

On Fedora they can be installed with:

    $ sudo dnf install git gcc-c++ cmake popt-devel libsigc++20-devel libusb1-devel fftw-devel \
                       fftw-libs-single alsa-lib-devel rtl-sdr-devel airspyone_host-devel

On Raspberry Pi OS/Debian/Ubuntu they can be installed with:

    $ sudo apt-get install git g++-8 cmake libpopt-dev libsigc++-2.0-dev libusb-1.0-0-dev \
                           libfftw3-dev libfftw3-single3 libasound2-dev librtlsdr-dev libairspy-dev


Download and build
====
The easiest way to get `sdrx` is to clone the GitHub repo.

> Note 1: _At the moment_, `sdrx` depend on the latest libairspy and librtlsdr
development/main branches from GitHub. Their respective source code is brought
into `sdrx` as git submodules. With the commands below, everything you need is
checked out properly.

> Note 2: This bundling of librtlsdr and libairspy into `sdrx` will in no way
interfere with what is allready installed on your system with respect to
librtlsdr and libairspy. The `sdrx` binary will not link to the on-system
librtlsdr and libairspy .so files.

> Note 3: The librtlsdr and libairspy packages still need to be installed on
your system to be able to run `sdrx` since they provide nessesary udev config
files for the unique USB ids that the dongles use.

Check out `sdrx` with the needed submodules:

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

The `--init` and `--recursive` arguments to `git submodule update` will assure
that newly added submodules are checked out properly. For existing submodules,
they are a no-op.

If the build for some reason fails after a update, try to remove all files
inside the build directory and start over:

    $ cd build
    $ rm -rf *
    $ cmake ../
    $ make

`sdrx` is under active development so make sure to `git pull` according
to the instruction above regularly to keep up with the changes. And always read
this README to see how the program changes over time as new features are added,
existing features are modified or features being removed.


Using
====
`sdrx` is run from the command line and takes an aeronautical channel as the
argument. Besides the channel, some options are available and can be listed
with `--help`:

    $ cd sdrx/build
    $ ./sdrx --help
    $ ./sdrx --gain 30 122.455

To stop the program, just press Crtl-C in the terminal and wait. This will stop
`sdr` cleanly as Ctrl-C is handled properly.

The defaults for volume and squelsh level should be good as is. RF gain
can be adjusted according to the local signal environment.

> Note 4: The R820T(2)/R860 tuner chip has three gain stages, LNA, Mixer and
VGA (sometimes referred to as IF). Each stage can be set to a value between 0
and 15 and each step represents a change in gain for the stage in question
(typically an increase about +3dB). The gain you give to `sdrx` as an
argument will be translated into one value between 0 and 15 for each stage
according to an `sdrx` internal mapping table. This table is the same as is
used in the official librtlsdr library. Look into the source code for the
details. Support for other ways of setting the gain will be introduced in the
near future.

`sdrx` use quite narrow filters so if your RTL dongle does not have a TCXO, take
your time to find out the proper frequency correction and supply that with the
`--fq-corr` option. For Airspy devices the correction concept is not used at
all and any `--fq-corr` given is silently ignored.

If you have multiple devices connected, use `--list` to list them:

    $ ./sdrx --list

To use a specific device, it's serial must be used and you must ensure that
all devices have unique serials. Use `rtl_eeprom -s MYSERIAL` from the standard
`librtlsdr` package to set unique serials for your RTL devices. Airspy devices
normaly have unique serials and you do not have to worry about them.

> Note 5: Airspy R2 devices are described as "AirSpy NOS" in the listing. This
is what they call themselves when queried over the USB bus and is nothing `sdrx`
can do anything about.

Support for multiple channels is available as well. Just specify the channels as
arguments (the channels must fit inside 80% of the sampling frequency used):

    $ ./sdrx --gain 40 118.105 118.280 118.405 118.505

The more channels you specify, the more processing power will be used. Please
monitor your system load when running `sdrx` with many channels to get an
understaning of how much you can load your specific system. Especially Airspy
devices combined with many channels consume quite some processing power at the
moment.

If the connection to a device is lost while `sdrx` is running, i.e. the device
is being unplugged from the USB bus, `sdrx` will auto reconnect when the device
is plugged in again. There is no need to restart the program just because a
device disappears for some reason. Some RTL based dongles have rather flimsy
USB connectors and a device easily disconnects by just moving it sligthly.

Sample rate defaults to 1.44MS/s for RTL devices and 6MS/s for Airspy devices,
if not set explicitly. Change to your liking with the `--sample-rate` option:

    $ ./sdrx --sample-rate 2.56 118.280 118.405 118.505

As stated earlier, the sample rate dictates the RF bandwidth that can be
used. If, for example, a sample rate of 2.56 MS/s is used, the available RF
bandwidth will be 2.56 * 0.8 = 2.048 MHz. For a rate of 1.44 MS/s it will be
1.44 * 0.8 = 1.152 MHz. And so on.

Available rates for each device is shown in the output from the `--list` option.

> Note 6: When specifying a sample rate, use the exact text of the rate as
shown when using `--list`, i.e. if the list say 2.56, you enter 2.56. If the list
say 0.96, you enter 0.96. If the list say 10, you enter 10. And so on. Do not
include "MS/s" after the rate value. Do not use comma (,) as decimal separator.
Do not try to set anything else other than what is shown with `--list`.


Output in single channel mode
====
Besides playing audio when the squelch is open, `sdrx` write signal power
measurements to the console while running at approximately three times a
second.

A typical output look like this:

    $ ./sdrx -g 45 118.105
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
desired squelch level to determine if the squelsh should be open or not.

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

    $ ./sdrx -g 40 118.105 118.280 118.405
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


Spurious outputs on the terminal
====
The `librtlsdr` library have quite some `fprintf:s` build in and pollutes the
terminal with printouts during start/stop and on different error cases.

The `--list` function will trigger some of them and the printouts may be
interpreted as a misbehaviour of `sdrx`, but it's not. To clean up the list
function, a redirect to `/dev/null` can help:

    $ ./sdrx --list 2>/dev/null

It's not advisable to run `sdrx` with stderr redirected to `/dev/null` all the
time since important errors will not be shown.

Ways of removing these spurious printouts are being considered.
