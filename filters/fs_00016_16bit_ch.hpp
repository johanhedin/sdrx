//
// FIR filter coefficients for the last 16kHz channelizer (pre demod)
//
// @author Johan Hedin
// @date   2021
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#ifndef FS_00016_16BIT_CH_HPP
#define FS_00016_16BIT_CH_HPP

#include <vector>

// Summary
//
// This file contain filters that can be used for the last channelization of
// 16kHz IQ baseband just before demodulation. The IQ stream is assumed to be
// the result of down sampling and is assumed to have ~99dB of dynamic range,
// a.k.a "16 bits".
//
// To calculate and plot the filters, three sdrx custom Octave functions are
// used; sincflt(), fltboxch() and vectortocpp(). These functions are included
// in the sdrx GitHub repository.


// Filter for last channelization just before AM demodulation.
//
// Care band is between 0 and 3.2kHz and we want the attenuation in the
// area above 5kHz to be more than 99dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 16;
//   > fcut = 3.2;
//   > frej = 5;
//   > arej = 99;
//   > c = sincflt(47, fs, fcut+0.5, @blackmanharris);
//
// Plot the filter response with the care band and rejct band marked with the
// fltboxch function:
//
//   > fltboxch(fs, fcut, frej, arej);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_00016_16bit_ch_amdemod_lpf1")
//
static const std::vector<float> fs_00016_16bit_ch_amdemod_lpf1 = {
     0.0000007540982051f,
     0.0000025392425633f,
    -0.0000155611362083f,
    -0.0000383153030636f,
     0.0000761351508689f,
     0.0002125357138395f,
    -0.0001950754548092f,
    -0.0007775988191939f,
     0.0002657955154443f,
     0.0021673267756859f,
     0.0001312705721833f,
    -0.0049212714221154f,
    -0.0019615779475231f,
     0.0094627979324318f,
     0.0069591723947720f,
    -0.0157842697297487f,
    -0.0179577598534563f,
     0.0231991872188193f,
     0.0402763406885340f,
    -0.0303526455262817f,
    -0.0902832785096816f,
     0.0355793074975081f,
     0.3127041682585321f,
     0.4625000452853876f,
     0.3127041682585321f,
     0.0355793074975091f,
    -0.0902832785096816f,
    -0.0303526455262821f,
     0.0402763406885340f,
     0.0231991872188193f,
    -0.0179577598534562f,
    -0.0157842697297488f,
     0.0069591723947720f,
     0.0094627979324318f,
    -0.0019615779475231f,
    -0.0049212714221154f,
     0.0001312705721833f,
     0.0021673267756859f,
     0.0002657955154444f,
    -0.0007775988191939f,
    -0.0001950754548092f,
     0.0002125357138396f,
     0.0000761351508689f,
    -0.0000383153030636f,
    -0.0000155611362083f,
     0.0000025392425633f,
     0.0000007540982051f
};

#endif // FS_00016_16BIT_CH_HPP
