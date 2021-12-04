Filter design
====
For the design of the filters in `sdrx`, Octave is used. This directory
contain Octave functions for various parts of the filter design and on this
page you will find various Octave "recipes" for filter desig.

At the moment, the following custom functions exist:

  * `sincflt` - A function for creating low pass FIR filters with windowed sinc.
  * `vectortocpp` - A function to print out at C++ float vector with filter coefficients.
  * `fltbox` - A function to plot filter responses toghether with pass and reject bands.
  * `fltboxch` - A funtion to plot filter response for a last channel filter.


Octave recipes
====
Below are some simple recipes for filter design relevant to `sdrx`:

Create a low pass filter with `remez`. All frequencies are in kHz:

    $ octave
    octave:> pkg load signal;
    octave:> fs = 1200;
    octave:> f = [ 0 5 200 fs/2 ];
    octave:> a = [ 1 1 0 0 ];
    octave:> w = [ 1 1 ];
    octave:> c = remez(12, f/(fs/2), a, w, 'bandpass', 26);
