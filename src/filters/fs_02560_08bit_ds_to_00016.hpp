//
// FIR filter coefficients for 2.56MS/s to 16kS/s downsampling filters
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

#ifndef FS_02560_08BIT_DS_TO_00016_HPP
#define FS_02560_08BIT_DS_TO_00016_HPP

#include <vector>

// Summary
//
// This file contain filters that can be used for downsampling a 2.56MS/s 8-bit
// IQ stream from a RTL dongle to 16kS/s. The dynamic range of the 2.56MS/s
// sample stream is assumed to be 50dB and we assume that we gain 3dB of dynamic
// range for every reduction of the sampling rate by two. E.g. a first
// downsampling stage with M = 2 increases the dynamic range from 50dB to 53dB.
// And so on (dynamic range increase = 10*log(M) dB).
//
// Going from 2.56MS/s to 16kS/s requires a total reduction factor M of 160.
// This can be achieved with four stages with M = 2, M = 4, M = 4 and M = 5 in
// any order. The resulting dynamic range is ~72dB.
//
// To calculate and plot the filters, three sdrx custom Octave functions are
// used; sincflt(), fltbox() and vectortocpp(). These functions are included in
// the sdrx GitHub repository.


// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 2.56MS/s to 640kS/s (M = 4) and increases the dynamic range from 50dB
// to ~56dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (640 +/-10 kHz and 1280 - 10 kHz in this case) to be more than 50dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 2560;
//   > fcut = 10;
//   > M = 4;
//   > c = sincflt(15, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 50, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf1_02560_to_00640")
//
static const std::vector<float> fs_02560_08bit_ds_lpf1_02560_to_00640 = {
     0.0000118919050313f,
     0.0008540640039329f,
     0.0066341767673193f,
     0.0258690590971228f,
     0.0662328374028934f,
     0.1236854871212659f,
     0.1771240683179702f,
     0.1991768307689284f,
     0.1771240683179702f,
     0.1236854871212659f,
     0.0662328374028934f,
     0.0258690590971229f,
     0.0066341767673193f,
     0.0008540640039329f,
     0.0000118919050313f
};


// Filter for second stage downsampling. Allows for reduction of the sample rate
// from 640kS/s to 160kS/s (M = 4) and increases the dynamic range from ~56dB
// to ~62dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (160 +/-10 kHz and 320 - 10 kHz in this case) to be more than 56dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 640;
//   > fcut = 10;
//   > M = 4;
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
//   > c = sincflt(17, fs, fcut, @blackmanharris);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 56, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf2_00640_to_00160")
//
static const std::vector<float> fs_02560_08bit_ds_lpf2_00640_to_00160 = {
     0.0000094852282211f,
     0.0004958669791918f,
     0.0035996941838971f,
     0.0139587159381900f,
     0.0372117884179930f,
     0.0752685051323650f,
     0.1213861968411337f,
     0.1602744168034998f,
     0.1755906609510169f,
     0.1602744168034998f,
     0.1213861968411337f,
     0.0752685051323651f,
     0.0372117884179930f,
     0.0139587159381900f,
     0.0035996941838971f,
     0.0004958669791918f,
     0.0000094852282211f
};


// Filter for third stage downsampling. Allows for reduction of the sample rate
// from 160kS/s to 32kS/s (M = 5) and increases the dynamic range from ~62dB
// to ~69dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// areas (32 +/- 5 kHz and 64 +/- 5 kHz in this case) to be more than 62dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 160;
//   > fcut = 5;
//   > M = 5;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-11-21:
//   > c = sincflt(31, fs, fcut+10, @chebwin, 62);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 62, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf3_00160_to_00032")
//
static const std::vector<float> fs_02560_08bit_ds_lpf3_00160_to_00032 = {
     0.0001840359044440f,
     0.0006409912293569f,
     0.0013774575329339f,
     0.0018053177501983f,
     0.0008400576094489f,
    -0.0026279207336757f,
    -0.0087212415427194f,
    -0.0154956721389694f,
    -0.0185683867043005f,
    -0.0121197466222812f,
     0.0087122316413160f,
     0.0449303707754600f,
     0.0917672243328019f,
     0.1390898117116958f,
     0.1744245369577410f,
     0.1875218645930988f,
     0.1744245369577410f,
     0.1390898117116958f,
     0.0917672243328019f,
     0.0449303707754600f,
     0.0087122316413160f,
    -0.0121197466222812f,
    -0.0185683867043005f,
    -0.0154956721389694f,
    -0.0087212415427194f,
    -0.0026279207336757f,
     0.0008400576094489f,
     0.0018053177501983f,
     0.0013774575329339f,
     0.0006409912293569f,
     0.0001840359044440f
};


// Filter for fourth stage downsampling. Allows for reduction of the sample rate
// from 32kS/s to 16kS/s (M = 2) and increases the dynamic range from ~69dB
// to ~72dB.
//
// Care band is between 0 and 5kHz and we want the attenuation in the folding
// area (16 - 5 kHz in this case) to be more than 69dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 32;
//   > fcut = 5;
//   > M = 2;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-11-21:
//   > c = sincflt(21, fs, fcut+2, @chebwin, 66);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 69, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf4_00032_to_00016")
//
static const std::vector<float> fs_02560_08bit_ds_lpf4_00032_to_00016 = {
     0.0003683349209640f,
    -0.0002660820012386f,
    -0.0035421569940826f,
    -0.0015090523618990f,
     0.0139246923242476f,
     0.0150906249368290f,
    -0.0329691913193094f,
    -0.0655928956025126f,
     0.0534609124936431f,
     0.3022619448294557f,
     0.4375457375478052f,
     0.3022619448294557f,
     0.0534609124936431f,
    -0.0655928956025126f,
    -0.0329691913193094f,
     0.0150906249368290f,
     0.0139246923242476f,
    -0.0015090523618990f,
    -0.0035421569940826f,
    -0.0002660820012386f,
     0.0003683349209640f
};





// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 2.56MS/s to 160kS/s (M = 16) and increases the dynamic range from 50dB
// to ~62.04dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (160 +/-10 kHz and 320 +/- 10 kHz and 960 +/- 10 in this case) to be
// more than 50dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 2560;
//   > fcut = 10;
//   > M = 16;
//   > c = sincflt(41, fs, fcut, @chebwin, 55);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 50, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf1_02560_to_00160")
//
static const std::vector<float> fs_02560_08bit_ds_lpf1_02560_to_00160 = {
     0.0015865468475583f,
     0.0019256484220172f,
     0.0030136569562523f,
     0.0044224582411319f,
     0.0061802610881581f,
     0.0083036600848512f,
     0.0107949105875284f,
     0.0136397767782353f,
     0.0168061584848598f,
     0.0202436625256841f,
     0.0238842287676967f,
     0.0276438534547813f,
     0.0314253774365601f,
     0.0351222303131845f,
     0.0386229491889759f,
     0.0418162285618006f,
     0.0445962111024823f,
     0.0468677018450225f,
     0.0485509832871791f,
     0.0495859270327776f,
     0.0499351379865260f,
     0.0495859270327776f,
     0.0485509832871791f,
     0.0468677018450225f,
     0.0445962111024823f,
     0.0418162285618006f,
     0.0386229491889759f,
     0.0351222303131845f,
     0.0314253774365601f,
     0.0276438534547813f,
     0.0238842287676967f,
     0.0202436625256841f,
     0.0168061584848598f,
     0.0136397767782353f,
     0.0107949105875284f,
     0.0083036600848512f,
     0.0061802610881581f,
     0.0044224582411319f,
     0.0030136569562523f,
     0.0019256484220172f,
     0.0015865468475583f
};


// Filter for first stage downsampling. Allows for reduction of the sample rate
// from 2.56MS/s to 128kS/s (M = 20) and increases the dynamic range from 50dB
// to ~63.01dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (160 +/-10 kHz and 320 +/- 10 kHz and 960 +/- 10 in this case) to be
// more than 50dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 2560;
//   > fcut = 10;
//   > M = 20;
//   > c = sincflt(51, fs, fcut, @chebwin, 55);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 50, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf1_02560_to_00128")
//
static const std::vector<float> fs_02560_08bit_ds_lpf1_02560_to_00128 = {
     0.0013769051701223f,
     0.0013482130854811f,
     0.0019750586352696f,
     0.0027591262887578f,
     0.0037145797662651f,
     0.0048524200510544f,
     0.0061796607077686f,
     0.0076985814719289f,
     0.0094061007314750f,
     0.0112933047756907f,
     0.0133451668367938f,
     0.0155404821412123f,
     0.0178520366829825f,
     0.0202470175978873f,
     0.0226876623091988f,
     0.0251321325549059f,
     0.0275355885508511f,
     0.0298514284607068f,
     0.0320326495746322f,
     0.0340332806311903f,
     0.0358098299554681f,
     0.0373226918261809f,
     0.0385374538951738f,
     0.0394260515953054f,
     0.0399677211763582f,
     0.0401497110546783f,
     0.0399677211763582f,
     0.0394260515953054f,
     0.0385374538951738f,
     0.0373226918261809f,
     0.0358098299554681f,
     0.0340332806311903f,
     0.0320326495746322f,
     0.0298514284607068f,
     0.0275355885508511f,
     0.0251321325549059f,
     0.0226876623091988f,
     0.0202470175978873f,
     0.0178520366829825f,
     0.0155404821412123f,
     0.0133451668367938f,
     0.0112933047756907f,
     0.0094061007314750f,
     0.0076985814719289f,
     0.0061796607077686f,
     0.0048524200510544f,
     0.0037145797662651f,
     0.0027591262887578f,
     0.0019750586352696f,
     0.0013482130854811f,
     0.0013769051701223f
};


// Filter for second stage downsampling. Allows for reduction of the sample rate
// from 128kS/s to 32kS/s (M = 4) and increases the dynamic range from ~63.01dB
// to ~69.03dB.
//
// Care band is between 0 and 10kHz and we want the attenuation in the folding
// areas (32 +/- 5 kHz and 64 +/- 5 kHz in this case) to be more than 63dB.
//
// Calculate with octave. fs is the sampling frequency and fcut is the care
// band high cutoff frequency (both in kHz):
//
//   > pkg load signal;
//   > fs = 128;
//   > fcut = 10;
//   > M = 4;
//   >
//   > % Candidates:
//   > c = sincflt(51, fs, fcut+2, @chebwin, 66);
//   >
//   > % Used as of 2021-12-19:
//   > c = sincflt(33, fs, fcut+4, @chebwin, 54);
//
// Plot the filter response with the care band and alias zones marked with the
// fltbox function:
//
//   > fltbox(fs, 63, fcut, M);
//   > plot((-0.5:1/4096:0.5-1/4096)*fs, 20*log10(abs(fftshift(fft(c, 4096)))), 'b-');
//
// Output the filter coefficients as a C++ float vector:
//
//   > vectortocpp(c, "fs_02560_08bit_ds_lpf2_00128_to_00032")
//
static const std::vector<float> fs_02560_08bit_ds_lpf2_00128_to_00032 = {
    -0.0006783399654280f,
    -0.0008093334100785f,
    -0.0003623383559927f,
     0.0014372195386501f,
     0.0043729475392083f,
     0.0067400722740468f,
     0.0056338795167298f,
    -0.0013947459080642f,
    -0.0138483793437282f,
    -0.0264765058542223f,
    -0.0298710715750712f,
    -0.0141285208821101f,
     0.0256718159836250f,
     0.0850333558412913f,
     0.1495910557035150f,
     0.1997504351731863f,
     0.2186769074488850f,
     0.1997504351731863f,
     0.1495910557035150f,
     0.0850333558412913f,
     0.0256718159836250f,
    -0.0141285208821101f,
    -0.0298710715750712f,
    -0.0264765058542223f,
    -0.0138483793437282f,
    -0.0013947459080642f,
     0.0056338795167298f,
     0.0067400722740468f,
     0.0043729475392083f,
     0.0014372195386501f,
    -0.0003623383559927f,
    -0.0008093334100785f,
    -0.0006783399654280f
};

#endif // FS_02560_08BIT_DS_TO_00016_HPP
