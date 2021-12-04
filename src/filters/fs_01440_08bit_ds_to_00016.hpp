//
// FIR filter coefficients for 1.44MS/s to 16kS/s downsampling filters
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

#ifndef FS_01440_08BIT_DS_TO_00016_HPP
#define FS_01440_08BIT_DS_TO_00016_HPP

#include <vector>

// Summary
//
// This file contain filters that can be used for downsampling a 1.44MS/s 8-bit
// IQ stream from a RTL dongle to 16kS/s. The dynamic range of the 1.44MS/s
// sample stream is assumed to be 50dB and we assume that we gain 3dB of dynamic
// range for every reduction of the sampling rate by two. E.g. a first
// downsampling stage with M = 2 increases the dynamic range from 50dB to 53dB.
// And so on (dynamic range increase = 10*log(M) dB).
//
// Going from 1.44MS/s to 16kS/s requires a total reduction factor M of 90. This
// can be achieved with tree stages with M = 3, M = 6 and M = 5 in any order.
// The resulting dynamic range is ~69.54dB.
//
// To calculate and plot the filters, three sdrx custom Octave functions are
// used; sincflt(), fltbox() and vectortocpp(). These functions are included in
// the sdrx GitHub repository.


// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 1.44MS/s to 480kS/s (M = 3) and increases the dynamic range from 50dB
// to ~54.8dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// area (480 +/- 10 kHz in this case) to be more than 50dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 1440;
//   > fcut = 10;
//   > M = 3;
//   > c = sincflt(13, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 50, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_01440_08bit_ds_lpf1_01440_to_00400")
//
static const std::vector<float> fs_01440_08bit_ds_lpf1_01440_to_00400 = {
     0.0000137905085407f,
     0.0015034725665646f,
     0.0128712414255828f,
     0.0504150055671368f,
     0.1208741328464410f,
     0.1980781199857339f,
     0.2324884742000007f,
     0.1980781199857339f,
     0.1208741328464410f,
     0.0504150055671368f,
     0.0128712414255828f,
     0.0015034725665646f,
     0.0000137905085407f
};


// Filter for second stage downsampling. Allows for reduction of the sample rate
// from 480kS/s to 80kS/s (M = 6) and increases the dynamic range from ~54.8dB
// to ~62.55dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (80 +/- 10 kHz and 160 +/- 10 kHz in this case) to be more than 55dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 480;
//   > fcut = 10;
//   > M = 6;
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
//   > % Used as of 2021-11-12:
//   > c = sincflt(23, fs, fcut+20, @chebwin, 50);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 55, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_01440_08bit_ds_lpf2_00480_to_00080")
//
static const std::vector<float> fs_01440_08bit_ds_lpf2_00480_to_00080 = {
    -0.0013194377234479f,
    -0.0019856318103831f,
    -0.0021156536696882f,
     0.0000000000000000f,
     0.0062341329167304f,
     0.0182037153447238f,
     0.0364705976129807f,
     0.0598867141176697f,
     0.0854254172320777f,
     0.1086928325547520f,
     0.1250414675774529f,
     0.1309316916942647f,
     0.1250414675774529f,
     0.1086928325547520f,
     0.0854254172320777f,
     0.0598867141176697f,
     0.0364705976129807f,
     0.0182037153447238f,
     0.0062341329167304f,
     0.0000000000000000f,
    -0.0021156536696882f,
    -0.0019856318103831f,
    -0.0013194377234479f
};


// Filter for third stage downsampling. Allows for reduction of the sample rate
// from 80kS/s to 16kS/s (M = 5) and increases the dynamic range from ~62.555dB
// to ~69.54dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// areas (16 +/- 5 kHz and 32 +/- 5 kHz in this case) to be more than 63dB.
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
//   > % Used as of 2021-11-21:
//   > c = sincflt(45, fs, fcut+2, @chebwin, 62);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 63, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_01440_08bit_ds_lpf3_00080_to_00016")
//
static const std::vector<float> fs_01440_08bit_ds_lpf3_00080_to_00016 = {
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


#endif // FS_01440_08BIT_DS_TO_00016_HPP
