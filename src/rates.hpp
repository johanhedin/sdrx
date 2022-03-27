//
// Sample rates om sdrx
//
// @author Johan Hedin
// @date   2022
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

#ifndef RATE_HPP
#define RATE_HPP

#include <string>
#include <cstdint>

// Supported sample rates in sdrx. Combination of what works for RTL dongles,
// Airspy devices and the internals of sdrx (M denotes downsampling factor to
// get to 16kHz).
enum class SampleRate {
    FS00960,      /// 0.96 MS/s RTL, M =  60
    FS01200,      //  1.2  MS/s RTL, M =  75
    FS01440,      //  1.44 MS/s RTL, M =  90
    FS01600,      //  1.6  MS/s RTL, M = 100
    FS01920,      //  1.92 MS/s RTL, M = 120
    FS02400,      //  2.4  MS/s RTL, M = 150
    FS02500,      //  2.5  MS/s Airspy R2
    FS02560,      //  2.56 MS/s RTL, M = 160
    FS03000,      //  3.0  MS/s Airspy Mini
    FS06000,      //  6.0  MS/s Airspy Mini (and R2 as alt. fs), M = 375
    FS10000,      // 10.0  MS/s Airspy R2 (and Mini as alt. fs), M = 625
    UNSPECIFIED
};


static inline const char *sample_rate_to_str(SampleRate rate) {
    switch (rate) {
        case SampleRate::FS00960: return "0.96";
        case SampleRate::FS01200: return "1.2";
        case SampleRate::FS01440: return "1.44";
        case SampleRate::FS01600: return "1.6";
        case SampleRate::FS01920: return "1.92";
        case SampleRate::FS02400: return "2.4";
        case SampleRate::FS02500: return "2.5";
        case SampleRate::FS02560: return "2.56";
        case SampleRate::FS03000: return "3";
        case SampleRate::FS06000: return "6";
        case SampleRate::FS10000: return "10";
        default:                  return "Unspecified";
    }
}


static inline SampleRate str_to_sample_rate(const std::string &str) {
    if      (str == "0.96") return SampleRate::FS00960;
    else if (str == "1.2")  return SampleRate::FS01200;
    else if (str == "1.44") return SampleRate::FS01440;
    else if (str == "1.6")  return SampleRate::FS01600;
    else if (str == "1.92") return SampleRate::FS01920;
    else if (str == "2.4")  return SampleRate::FS02400;
    else if (str == "2.5")  return SampleRate::FS02500;
    else if (str == "2.56") return SampleRate::FS02560;
    else if (str == "3")    return SampleRate::FS03000;
    else if (str == "6")    return SampleRate::FS06000;
    else if (str == "10")   return SampleRate::FS10000;
    else                    return SampleRate::UNSPECIFIED;
}


static inline uint32_t sample_rate_to_uint(SampleRate rate) {
    switch (rate) {
        case SampleRate::FS00960: return   960000;
        case SampleRate::FS01200: return  1200000;
        case SampleRate::FS01440: return  1440000;
        case SampleRate::FS01600: return  1600000;
        case SampleRate::FS01920: return  1920000;
        case SampleRate::FS02400: return  2400000;
        case SampleRate::FS02500: return  2500000;
        case SampleRate::FS02560: return  2560000;
        case SampleRate::FS03000: return  3000000;
        case SampleRate::FS06000: return  6000000;
        case SampleRate::FS10000: return 10000000;
        default:                  return        0;
    }
}


static inline SampleRate uint_to_sample_rate(uint32_t value) {
    if      (value ==   960000) return SampleRate::FS00960;
    else if (value ==  1200000) return SampleRate::FS01200;
    else if (value ==  1440000) return SampleRate::FS01440;
    else if (value ==  1600000) return SampleRate::FS01600;
    else if (value ==  1920000) return SampleRate::FS01920;
    else if (value ==  2400000) return SampleRate::FS02400;
    else if (value ==  2500000) return SampleRate::FS02500;
    else if (value ==  2560000) return SampleRate::FS02560;
    else if (value ==  3000000) return SampleRate::FS03000;
    else if (value ==  6000000) return SampleRate::FS06000;
    else if (value == 10000000) return SampleRate::FS10000;
    else                        return SampleRate::UNSPECIFIED;
}

#endif // RATE_HPP
