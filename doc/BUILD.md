# Download and build sdrx
`sdrx` is only available in source code form and the easiest way to get it
is to clone the GitHub repo and compile the code using the instructions on
this page.


## Build requirements
Besides git, gcc, cmake and the standard set of development tools, `sdrx` depend
on the following libraries. They, and their development parts, need to be
installed on your machine in order to build and run `sdrx`:

 * popt
 * libsigc++20
 * libusb-1.0
 * fftw
 * alsa
 * librtlsdr
 * libairspy
 * libuv
 * zlib
 * openssl

On Fedora they can be installed with:

```console
sudo dnf install git gcc-c++ cmake popt-devel libsigc++20-devel libusb1-devel \
fftw-devel fftw-libs-single alsa-lib-devel rtl-sdr-devel libuv-devel \
airspyone_host-devel zlib-devel libuv-devel openssl-devel
```

On Raspberry Pi OS/Debian/Ubuntu they can be installed with:

```console
sudo apt-get install git g++ cmake build-essential libpopt-dev libsigc++-2.0-dev \
libusb-1.0-0-dev libfftw3-dev libfftw3-single3 libasound2-dev libuv1-dev \
librtlsdr-dev libairspy-dev zlib1g-dev libuv1-dev libssl-dev
```


## Clone the repo and build
> Note 1: At the moment `sdrx` depend on the latest libairspy and librtlsdr
development/main branches from GitHub. Their respective source code is brought
into the `sdrx` source tree as git submodules. With the commands below,
everything you need is checked out properly.

> Note 2: This bundling of the librtlsdr and libairspy sources into `sdrx` will
in no way interfere with what is allready installed on your system with respect
to librtlsdr and libairspy. The `sdrx` binary will not link to the on-system
librtlsdr and libairspy .so files.

> Note 3: The librtlsdr and libairspy packages still need to be installed on
your system to be able to run `sdrx` since they provide the nessesary udev config
files for the unique USB ids that the dongles use.

Clone `sdrx` from GitHub with the needed submodules:

```console
git clone --recurse-submodules https://github.com/johanhedin/sdrx.git
cd sdrx
```

and then build with cmake:

```console
mkdir build
cd build
cmake ..
make
```


## Keep up to date with changes
To keep up to date with changes and updates to `sdrx`, simply run:

```console
cd sdrx
git pull --ff-only
git submodule update --init --recursive
cd build
cmake ..
make
```

Please always run the `git submodule update`-part as stated above since it is
required for the submodule linking to work as indended. The `--init` and
`--recursive` arguments will assure that newly added submodules are checked out
properly. For existing submodules, they are a no-op.

If the build for some reason fails after a update, try to remove all files
inside the build directory and start over:

```console
cd build
rm -rf *
cmake ..
make
```

Make sure to `git pull` according to the instruction above regularly to keep up
with the changes. And always read the README to see how the program changes
over time as new features are added, existing features are modified or features
being removed.


## Install/uninstall
If you like, you can install the `sdrx` executable on your system. Install
directory defaults to `/usr/local/bin`. It can be set to a different
location with `-DCMAKE_INSTALL_PREFIX=<your_custom_dir>`:

```console
cd build
rm -rf *
cmake .. -DCMAKE_INSTALL_PREFIX=/usr
make
sudo make install
```

To uninstall, you need to install like above first and have the build directory
available as it was after the install (the files installed are written to
a file in the build directory and then used in the uninstall stage):

```console
cd build
sudo make uninstall
```

`sudo` is only needed if your install directory is owned by root.


## Using `sdrx`
Instruction for how to use `sdrx` can be found on the [usage](USING.md) page.
