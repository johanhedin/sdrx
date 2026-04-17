//
// Sample rates in sdrx
//
// @author Johan Hedin
// @date   2022, 2026
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

#ifndef RATES_HPP
#define RATES_HPP

#include <array>
#include <cstdint>
#include <string_view>

// Supported sample rates in sdrx. Combination of what works for RTL dongles,
// Airspy devices and the internals of sdrx (M denotes downsampling factor to
// get to 16kHz).
enum class SampleRate : uint32_t {
    FS00960     =   960000,  // 0.96 MS/s RTL, M =  60
    FS01200     =  1200000,  //  1.2 MS/s RTL, M =  75
    FS01440     =  1440000,  // 1.44 MS/s RTL, M =  90
    FS01600     =  1600000,  //  1.6 MS/s RTL, M = 100
    FS01920     =  1920000,  // 1.92 MS/s RTL, M = 120
    FS02400     =  2400000,  //  2.4 MS/s RTL, M = 150
    FS02500     =  2500000,  //  2.5 MS/s Airspy R2
    FS02560     =  2560000,  // 2.56 MS/s RTL, M = 160
    FS03000     =  3000000,  //  3.0 MS/s Airspy Mini
    FS06000     =  6000000,  //  6.0 MS/s Airspy Mini (and R2 as alt. fs), M = 375
    FS10000     = 10000000,  // 10.0 MS/s Airspy R2 (and Mini as alt. fs), M = 625
    UNSPECIFIED =        0
};

namespace detail {
    struct SampleRateEntry { SampleRate rate; std::string_view str; };
    inline constexpr std::array<SampleRateEntry, 11> sample_rates = {{
        { SampleRate::FS00960, "0.96" },
        { SampleRate::FS01200, "1.2"  },
        { SampleRate::FS01440, "1.44" },
        { SampleRate::FS01600, "1.6"  },
        { SampleRate::FS01920, "1.92" },
        { SampleRate::FS02400, "2.4"  },
        { SampleRate::FS02500, "2.5"  },
        { SampleRate::FS02560, "2.56" },
        { SampleRate::FS03000, "3"    },
        { SampleRate::FS06000, "6"    },
        { SampleRate::FS10000, "10"   },
    }};
}

[[nodiscard]] constexpr std::string_view sample_rate_to_str(SampleRate rate) noexcept {
    for (const auto& sample_rate : detail::sample_rates)
        if (sample_rate.rate == rate) return sample_rate.str;
    return "Unspecified";
}

[[nodiscard]] constexpr SampleRate str_to_sample_rate(std::string_view str) noexcept {
    for (const auto& sample_rate : detail::sample_rates)
        if (sample_rate.str == str) return sample_rate.rate;
    return SampleRate::UNSPECIFIED;
}

[[nodiscard]] constexpr uint32_t sample_rate_to_uint(SampleRate rate) noexcept {
    return static_cast<uint32_t>(rate);
}

[[nodiscard]] constexpr SampleRate uint_to_sample_rate(uint32_t value) noexcept {
    for (const auto& sample_rate : detail::sample_rates)
        if (static_cast<uint32_t>(sample_rate.rate) == value) return sample_rate.rate;
    return SampleRate::UNSPECIFIED;
}

#endif // RATES_HPP
