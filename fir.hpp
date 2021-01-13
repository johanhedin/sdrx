//
// FIR filter with additional gain.
//
// @author Johan Hedin
// @date   2021-01-13
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

#ifndef FIR_HPP
#define FIR_HPP

#include <vector>
#include <cmath>

class FIR {
public:
    FIR(const std::vector<float> c) : c_(c), c_adj_(c), buf_(c.size(), 0.0f), buf_pos_(buf_.begin()), gain_(0.0f) {}

    // Filter data from in and write to out. in and out may point to the same array
    void filter(const float *in, unsigned in_len, float *out) {
        for (auto sample = in; sample != in + in_len; ++sample) {
            // Write in sample to internal ring buffer.
            *buf_pos_ = *sample;

            // Advance and wrap around if necessary
            if (++buf_pos_ == buf_.end()) buf_pos_ = buf_.begin();

            // Calculate out sample
            *out = 0.0f;
            for (auto c = c_adj_.begin(); c != c_adj_.end(); ++c) {
                *out += *c * *buf_pos_;
                if (++buf_pos_ == buf_.end()) buf_pos_ = buf_.begin();
            }

            out++;
        }
    }

    // Set filter gain (in dB)
    void setGain(float gain) {
        gain_ = gain;

        auto c = c_.begin();
        auto c_adj = c_adj_.begin();
        while (c != c_.end()) {
            *c_adj = *c * std::pow(10.0f, gain_/20.0f);;
            ++c;
            ++c_adj;
        }
    }

    // Get filter gain (in dB)
    const float gain(void) { return gain_; }

private:
    const std::vector<float>     c_;       // FIR coefficients
    std::vector<float>           c_adj_;   // FIR coefficients adjusted for gain
    std::vector<float>           buf_;     // Ring buffer delay line
    std::vector<float>::iterator buf_pos_; // Position in ring buffer
    float                        gain_;    // Filter gain in dB
};

#endif // FIR_HPP
