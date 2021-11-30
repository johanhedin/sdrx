//
// FIR filter coefficients for 2.4MS/s to 16kS/s downsampling filters
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

#ifndef FS_02400_08BIT_DS_TO_00016_HPP
#define FS_02400_08BIT_DS_TO_00016_HPP

#include <vector>

// Summary
//
// This file contain filters that can be used for downsampling a 2.4MS/s 8-bit
// IQ stream from a RTL dongle to 16kS/s. The dynamic range of the 2.4MS/s
// sample stream is assumed to be 50dB and we assume that we gain 3dB of dynamic
// range for every reduction of the sampling rate by two. E.g. a first
// downsampling stage with M = 2 increases the dynamic range from 50dB to 53dB.
// And so on (dynamic range increase = 10*log(M) dB).
//
// Going from 2.4MS/s to 16kS/s requires a total reduction factor M of 150. This
// can be achieved with four stages with M = 2, M = 3, M = 5 and M = 5 in any order.
// The resulting dynamic range is ~71.76dB.
//
// To calculate and plot the filters, three sdrx custom Octave functions are
// used; sincflt(), fltbox() and vectortocpp(). These functions are included in
// the sdrx GitHub repository.


// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 2.4MS/s to 1.2MS/s (M = 2) and increases the dynamic range from 50dB
// to 53dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// area (1200 - 10 kHz in this case) to be more than 50dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 2400;
//   > fcut = 10;
//   > M = 2;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-11-30:
//   > c = sincflt(9, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 50, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02400_08bit_ds_lpf1_02400_to_01200")
//
static const std::vector<float> fs_02400_08bit_ds_lpf1_02400_to_01200 = {
     0.0000208702254657f,
     0.0075665817541964f,
     0.0757479421225900f,
     0.2424277814848310f,
     0.3484736488258336f,
     0.2424277814848310f,
     0.0757479421225901f,
     0.0075665817541964f,
     0.0000208702254657f
};


// Filter for second stage downsampling. Allows for reduction of the sample rate
// from 1.2MS/s to 400kS/s (M = 3) and increases the dynamic range from 53dB
// to ~57.77dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// area (400 +/- 10 kHz in this case) to be more than 53dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 1200;
//   > fcut = 10;
//   > M = 3;
//   > c = sincflt(13, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 53, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02400_08bit_ds_lpf2_01200_to_00400")
//
static const std::vector<float> fs_02400_08bit_ds_lpf2_01200_to_00400 = {
     0.0000137262713506f,
     0.0014987912638232f,
     0.0128474010569802f,
     0.0503710213693594f,
     0.1208531831464377f,
     0.1981268215399144f,
     0.2325781107042688f,
     0.1981268215399144f,
     0.1208531831464378f,
     0.0503710213693595f,
     0.0128474010569802f,
     0.0014987912638232f,
     0.0000137262713506f
};


// Filter for third stage downsampling. Allows for reduction of the sample rate
// from 400kS/s to 80kS/s (M = 5) and increases the dynamic range from ~57.77dB
// to ~64.76dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (80 +/- 10 kHz and 160 +/- 10 kHz in this case) to be more than 58dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 400;
//   > fcut = 10;
//   > M = 5;
//   >
//   > % Candidates:
//   > c = sincflt(23, fs, fcut, @blackmanharris);
//   > c = sincflt(23, fs, fcut, @blackmannuttall, 'symmetric');
//   > c = sincflt(23, fs, fcut+15, @gaussian, 0.23);
//   > c = sincflt(23, fs, fcut, @nuttallwin, 'symmetric');
//   > c = sincflt(15, fs, fcut, @chebwin, 55);
//   > c = sincflt(21, fs, fcut, @parzenwin);
//   > c = sincflt(21, fs, fcut, @ultrwin, 1, 4.0);
//   > c = sincflt(17, fs, fcut, @ultrwin, 1, 3.0);
//   > c = sincflt(17, fs, fcut+10, @ultrwin, 1, 2.4);
//   >
//   > % Used as of 2021-11-30:
//   > c = sincflt(19, fs, fcut+17, @chebwin, 55);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 58, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02400_08bit_ds_lpf3_00400_to_00080")
//
static const std::vector<float> fs_02400_08bit_ds_lpf3_00400_to_00080 = {
    -0.0007688947906962f,
    -0.0008552256209725f,
     0.0013755646551764f,
     0.0090509083036270f,
     0.0251867984797878f,
     0.0508151453065983f,
     0.0833037373908732f,
     0.1161405747825238f,
     0.1407793252320492f,
     0.1499441325220660f,
     0.1407793252320492f,
     0.1161405747825238f,
     0.0833037373908732f,
     0.0508151453065983f,
     0.0251867984797878f,
     0.0090509083036270f,
     0.0013755646551764f,
    -0.0008552256209725f,
    -0.0007688947906962f
};


// Filter for fourth stage downsampling. Allows for reduction of the sample rate
// from 80kS/s to 16kS/s (M = 5) and increases the dynamic range from ~64.76dB
// to ~71.75dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// areas (16 +/- 5 kHz and 32 +/- 5 kHz in this case) to be more than 65dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 80;
//   > fcut = 5;
//   > M = 5;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-11-30:
//   > c = sincflt(45, fs, fcut+2, @chebwin, 62);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 65, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02400_08bit_ds_lpf4_00080_to_00016")
//
static const std::vector<float> fs_02400_08bit_ds_lpf4_00080_to_00016 = {
    -0.0001088165129735f,
    -0.0002922217387049f,
    -0.0005887023450137f,
    -0.0008045434010244f,
    -0.0006527503605381f,
     0.0001652849383831f,
     0.0017579005715384f,
     0.0038218611111694f,
     0.0055297942065796f,
     0.0056573665307528f,
     0.0030087113881180f,
    -0.0029373562717237f,
    -0.0113854047030713f,
    -0.0198953063408979f,
    -0.0246359672946174f,
    -0.0213062868068592f,
    -0.0065377910951878f,
     0.0206586797936201f,
     0.0579535316993348f,
     0.0997172140488533f,
     0.1381448424280100f,
     0.1652312621394781f,
     0.1749973960295491f,
     0.1652312621394781f,
     0.1381448424280100f,
     0.0997172140488532f,
     0.0579535316993347f,
     0.0206586797936201f,
    -0.0065377910951878f,
    -0.0213062868068592f,
    -0.0246359672946174f,
    -0.0198953063408979f,
    -0.0113854047030713f,
    -0.0029373562717237f,
     0.0030087113881180f,
     0.0056573665307528f,
     0.0055297942065796f,
     0.0038218611111694f,
     0.0017579005715384f,
     0.0001652849383831f,
    -0.0006527503605381f,
    -0.0008045434010244f,
    -0.0005887023450137f,
    -0.0002922217387049f,
    -0.0001088165129735f
};

#endif // FS_02400_08BIT_DS_TO_00016_HPP
