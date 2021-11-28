//
// FIR filter coefficients for 6.0MS/s to 16kS/s downsampling filters
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

#ifndef FS_06000_12BIT_DS_TO_00016_HPP
#define FS_06000_12BIT_DS_TO_00016_HPP

#include <vector>

// Summary
//
// This file contain filters that can be used for downsampling a 6.0MS/s 12-bit
// IQ stream from a Airspy Mini or R2 to 16kS/s. The dynamic range of the
// 6.0MS/s sample stream is assumed to be 74dB and we assume that we gain 3dB
// of dynamic range for every reduction of the sampling rate by two. E.g. a
// first downsampling stage with M = 2 increases the dynamic range from 74dB to
// 77dB. And so on (dynamic range increase = 10*log(M) dB).
//
// Going from 6.0MS/s to 16kS/s requires a total reduction factor M of 375.
// This can be achieved with four stages with M = 3, M = 5, M = 5 and M = 5 in
// any order. The resulting dynamic range is ~100dB.
//
// To calculate and plot the filters, three sdrx custom Octave functions are
// used; sincflt(), fltbox() and vectortocpp(). These functions are included in
// the sdrx GitHub repository.


// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 6.0MS/s to 2.0MS/s (M = 3) and increases the dynamic range from 74dB
// to ~78.8dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// area (2000 +/-10 kHz in this case) to be more than 74dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 6000;
//   > fcut = 10;
//   > M = 3;
//   > c = sincflt(13, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 74, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_06000_12bit_ds_lpf1_06000_to_02000")
//
static const std::vector<float> fs_06000_12bit_ds_lpf1_06000_to_02000 = {
     0.0000139286228928f,
     0.0015135228222827f,
     0.0129223607004594f,
     0.0505092169244261f,
     0.1209189253347150f,
     0.1979737807440622f,
     0.2322965297023235f,
     0.1979737807440622f,
     0.1209189253347151f,
     0.0505092169244261f,
     0.0129223607004594f,
     0.0015135228222827f,
     0.0000139286228928f
};


// Filter for second stage downsampling. Allows for reduction of the sample rate
// from 2000kS/s to 400kS/s (M = 5) and increases the dynamic range from ~78.8dB
// to ~85.8dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (400 +/-10 kHz and 800 +/- 10 kHz in this case) to be more than 79dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 2000;
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
//   > % Used as of 2021-11-12:
//   > c = sincflt(17, fs, fcut, @ultrwin, 1, 3.33);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 79, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_06000_12bit_ds_lpf2_02000_to_00400")
//
static const std::vector<float> fs_06000_12bit_ds_lpf2_02000_to_00400 = {
     0.0005956268032525f,
     0.0033731233862635f,
     0.0110224214658969f,
     0.0264261198982120f,
     0.0508435957344173f,
     0.0820789193805866f,
     0.1139808044757884f,
     0.1381142323780024f,
     0.1471303129551607f,
     0.1381142323780024f,
     0.1139808044757884f,
     0.0820789193805866f,
     0.0508435957344173f,
     0.0264261198982120f,
     0.0110224214658969f,
     0.0033731233862635f,
     0.0005956268032525f
};


// Filter for third stage downsampling. Allows for reduction of the sample rate
// from 400kS/s to 80kS/s (M = 5) and increases the dynamic range from ~85.8dB
// to ~92.75dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// areas (80 +/- 5 kHz and 160 +/- 5 kHz in this case) to be more than 86dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 400;
//   > fcut = 5;
//   > M = 5;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-11-21:
//   > c = sincflt(21, fs, fcut+10, @chebwin, 80);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 86, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_06000_12bit_ds_lpf3_00400_to_00800")
//
static const std::vector<float> fs_06000_12bit_ds_lpf3_00400_to_00800 = {
     0.0001748744558828f,
     0.0009842833658397f,
     0.0034403805150893f,
     0.0090638480673209f,
     0.0195195775455838f,
     0.0358506948752110f,
     0.0575856472944005f,
     0.0821901120091661f,
     0.1053080134685091f,
     0.1219061709211306f,
     0.1279527949637326f,
     0.1219061709211306f,
     0.1053080134685091f,
     0.0821901120091661f,
     0.0575856472944005f,
     0.0358506948752110f,
     0.0195195775455838f,
     0.0090638480673209f,
     0.0034403805150893f,
     0.0009842833658397f,
     0.0001748744558828f
};


// Filter for fourth stage downsampling. Allows for reduction of the sample rate
// from 80kS/s to 16kS/s (M = 5) and increases the dynamic range from ~92.75dB
// to ~99.74dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// areas (16 +/- 5 kHz and 32 +/- 5 kHz in this case) to be more than 93dB.
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
//   > c = sincflt(67, fs, fcut+2, @chebwin, 90);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 93, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_06000_12bit_ds_lpf4_00080_to_00016")
//
static const std::vector<float> fs_06000_12bit_ds_lpf4_00080_to_00016 = {
    -0.0000064471239389f,
    -0.0000176961232757f,
    -0.0000348774319277f,
    -0.0000447046683118f,
    -0.0000244229288813f,
     0.0000509930097215f,
     0.0001904865352041f,
     0.0003639109145100f,
     0.0004874307139512f,
     0.0004339065220899f,
     0.0000794157943688f,
    -0.0006188871674095f,
    -0.0015405779778606f,
    -0.0023606684964849f,
    -0.0025959847790922f,
    -0.0017620690079488f,
     0.0003841545852356f,
     0.0035966197528846f,
     0.0070064486427414f,
     0.0092166561548568f,
     0.0086781161449435f,
     0.0042921835730552f,
    -0.0039326979156883f,
    -0.0144213419005686f,
    -0.0240132679357701f,
    -0.0285194086991557f,
    -0.0237979767660383f,
    -0.0070847784871680f,
     0.0218328631707456f,
     0.0600248125877290f,
     0.1016952657598081f,
     0.1393518194612640f,
     0.1655912338474367f,
     0.1749989804779492f,
     0.1655912338474367f,
     0.1393518194612638f,
     0.1016952657598078f,
     0.0600248125877290f,
     0.0218328631707456f,
    -0.0070847784871680f,
    -0.0237979767660384f,
    -0.0285194086991557f,
    -0.0240132679357701f,
    -0.0144213419005686f,
    -0.0039326979156883f,
     0.0042921835730552f,
     0.0086781161449435f,
     0.0092166561548568f,
     0.0070064486427414f,
     0.0035966197528846f,
     0.0003841545852356f,
    -0.0017620690079488f,
    -0.0025959847790922f,
    -0.0023606684964849f,
    -0.0015405779778606f,
    -0.0006188871674095f,
     0.0000794157943688f,
     0.0004339065220899f,
     0.0004874307139512f,
     0.0003639109145100f,
     0.0001904865352041f,
     0.0000509930097215f,
    -0.0000244229288813f,
    -0.0000447046683118f,
    -0.0000348774319277f,
    -0.0000176961232757f,
    -0.0000064471239389f
};


#endif // FS_06000_12BIT_DS_TO_00016_HPP
