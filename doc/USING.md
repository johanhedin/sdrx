# Using sdrx
`sdrx` is run from the command line in a terminal window. If you have not already
build `sdrx`, look at the [build](BUILD.md) page for instructions.


## Basic usage
`sdrx` takes an aeronautical channel to listen to as the argument. Besides the
channel, options are available and can be listed with `--help` (his page
does not always cover all available options so use this page together with
`--help` to get the full picture):

```console
cd sdrx/build
./sdrx --help
./sdrx --gain 30 122.455
```

To stop the program, just press Crtl-C in the terminal and wait. This will stop
`sdr` cleanly as Ctrl-C is handled properly. If you have multiple devices
connected you can easily run multiple instances of `sdrx` in different terminal
windows.

The defaults for volume and squelsh level should be good as is. RF gain
can be adjusted according to the local signal environment.

The R820T(2)/R860 tuner chip has three gain stages, LNA, Mixer and VGA (sometimes
referred to as IF). Each stage can be set to a value between 0 and 15 and each
increment of the value represents a change in gain (typically an increase of
about +3dB). The gain you give to `sdrx` as an argument will be translated into
one value between 0 and 15 for each stage according to an `sdrx` internal
mapping table. This mapping table is the same as is used in the official
librtlsdr library (look in the source code for the details). As an alternative,
it is also possible to set the three gain stages directly with stage values
like this (LNA = 5, MIX = 8 and VGA = 10):

```console
./sdrx --gain 5:8:10 122.455
```

This gives very good control over how the total gain is distributed in the R820
tuner and is the preferred way of setting gain when you run a external LNA in
front of your device.

`sdrx` use quite narrow filters so if your RTL dongle does not have a TCXO, take
your time to find out the proper frequency correction and supply that with the
`--fq-corr` option. For Airspy devices the correction concept is not used at
all and any `--fq-corr` given is silently ignored.

If you have multiple devices connected, use `--list` to list what devices that
`sdrx` recognize on your system and what sample rates they support:

```console
./sdrx --list
```

To use a specific device, it's serial is used and you must ensure that all
devices have unique serials. Use `rtl_eeprom -s MYSERIAL` from the standard
`librtlsdr` package to set unique serials for your RTL devices. Airspy devices
normaly have unique serials and you do not have to worry about them.

> Note 1: Unlike many other programs that support RTL and/or Airspy dongles,
`sdrx` does not use the "device id" concept at all. An "id" (typically a low
number like 0 or 1) is not a stable way to reference a dongle since the id
may change over time as devices are plugged in and removed from the USB bus.
The serial number concept is, on the other hand, a stable and predictive way
to reference a specific dongle as long as every dongle on the system have been
given a unique name.

> Note 2: The term "serial" is a bit misleading since it actually is a text
string based on a USB descriptor. It is prefectly fine to set a serial on a RTL
device containing text.

> Note 3: Airspy R2 devices are described as "AirSpy NOS" when listing available
devices. This is what they call themselves when queried over the USB bus and is
nothing `sdrx` can do anything about.

Support for multiple channels is available as well. Just specify the channels as
arguments. The channels must fit inside 80% of the sampling frequency used (see
below for explanation):

```console
./sdrx --gain 40 118.105 118.280 118.405 118.505
```

The more channels you specify, the more loaded the channelization thread will be.
Please monitor your system load when running `sdrx` with many channels to get an
understaning of how much you can load your specific system. Especially Airspy
devices combined with many channels consume quite some processing power at the
moment.

If the connection to a device is lost while `sdrx` is running, i.e. the device
is being unplugged from the USB bus, `sdrx` will auto reconnect when the device
is plugged in again. There is no need to restart the program just because a
device disappears for some reason. Some RTL based dongles have rather flimsy
USB connectors and a device easily disconnects by just moving it sligthly.

Sample rate defaults to 1.44MS/s for RTL devices and 6MS/s for Airspy devices
if not set explicitly. Change to your liking with the `--sample-rate` option:

```console
./sdrx --sample-rate 2.56 118.280 118.405 118.505
```

As stated earlier, the sample rate dictates the RF bandwidth that can be
used. If, for example, a sample rate of 2.56 MS/s is used, the available RF
bandwidth will be 2.56 * 0.8 = 2.048 MHz. For a rate of 1.44 MS/s it will be
1.44 * 0.8 = 1.152 MHz. And so on.

Available rates for each device is shown in the output from the `--list` option.

> Note 4: When specifying a sample rate, use the exact text of the rate as
shown when using `--list`, i.e. if the list say 2.56, you enter 2.56. If the list
say 0.96, you enter 0.96. If the list say 10, you enter 10. And so on. Do not
include "MS/s" after the rate value. Do not use comma (,) as decimal separator.
Do not set anything else other than what is shown with `--list`.


## Output in single channel mode
Besides playing audio when the squelch is open, `sdrx` write signal power
measurements to the console while running at approximately three times a
second.

A typical output look like this:

```console
./sdrx -g 45 118.105
...
10:57:00: Level[X   -37.2] 118.105[ 0.0] [-22.5|-22.6|-23.4] [  0.00] [SNR][low|mid|hig][imbalance]
10:57:01: Level[X   -39.6] 118.105[ 0.0] [-22.1|-22.9|-22.2] [  0.00] [SNR][low|mid|hig][imbalance]
10:57:01: Level[X   -39.5] 118.105[ 0.0] [-21.1|-22.3|-22.8] [  0.00] [SNR][low|mid|hig][imbalance]
10:57:01: Level[XXX -27.5] 118.105[38.0] [-21.4| 16.5|-21.7] [-12.28] [SNR][low|mid|hig][imbalance]
10:57:02: Level[XXX -27.5] 118.105[38.7] [-22.7| 16.6|-21.6] [-30.74] [SNR][low|mid|hig][imbalance]
10:57:02: Level[XXX -27.5] 118.105[39.7] [-21.8| 16.9|-24.2] [-30.66] [SNR][low|mid|hig][imbalance]
...
```

Open squelch is indicated by the channel name getting a yellow background.
**Level** is a measurement of the signal level before the ADC in dB (dBFS) and
can be used to see if the receiver is oversteered at the IF/ADC stage or not.

**mid** is the power level in dB for the center band of the channel you are
listenting to, i.e fc +/- 2.8kHz and **low** and **hig** are the power levels
for the bands just outside of the channel, i.e. fc - 3.5-4.9kHz and fc + 3.5-4.9kHz.

The noise floor for the channel is estimated as the power just outside of where
the modulated audio resides (i.e. the **low** and **hig** measurements). **SNR**
is the difference between "signal power" and "noise power" and is checked against the
desired squelch level to determine if the squelsh should be open or not.

**imbalance** is a measure of how "off" the receiver is compared to the frequency
that the transmitter is using. If it is negative, you are tuned above the
transmitter and if it is positive you are tuned below the transmitter frequency.

The imbalance is determined by calculating the energy balance between "negative"
and "positive" frequencies from the FFT used for the squelch. Since AM is a
symmetric modulation around the carrier, the imbalance should be 0 if you are
tuned to the transmitters frequency.


## Output in multi channel mode
In multi channel mode, audio from the different channels are distributed between
the left and right speaker in a virtual five speaker audio panorama. This will
create the sense of a set of five speakers front of the listener.

Together with the visual presentation in the terminal, this will increase the
awareness of what channels that are active. The channels will be placed in the
panorma based on their order on the command line.

Output in the terminal looks about the same as for single channel but only the
SNR is shown for each individual channel:

```console
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
```
