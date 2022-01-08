//
// Simple IF AGC to be used just before demodulation
//
// @author Johan Hedin
// @date   2021-01-31
//
// Implementation is based on the AGC in svxlink: https://github.com/sm0svx/svxlink
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

#ifndef AGC_HPP
#define AGC_HPP

#include <cmath>

class AGC {
public:
    AGC(float attack=10.0f, float decay=0.01f, float reference=0.25f, float max_gain=200.0f)
    : attack_(attack), decay_(decay), reference_(reference), max_gain_(max_gain), gain_(1.0f) {}

    void setAttack(float attack) { attack_ = attack; }
    void setDecay(float decay) { decay_ = decay; }
    void setReference(float reference) { reference_ = reference; }
    void setMaxGain(float max_gain) { max_gain_ = max_gain; }

    float gain(void) { return gain_; }

    iqsample_t adjust(iqsample_t sample) {
        iqsample_t sample_adjusted = sample * gain_;

        float power = std::norm(sample_adjusted);
        float error = reference_ - power;

        if (error > 0.0f) {
            // Adjusted signal power is under reference. Increase gain
            gain_ += decay_ * error;
        } else {
            // Adjusted signal power is over reference. Decrease gain
            gain_ += attack_ * error;
        }

        if (gain_ < 0.0f) {
            gain_ = 0.0f;
        } else if (gain_ > max_gain_) {
            gain_ = max_gain_;
        }

        return sample_adjusted;
    }

private:
    float   attack_;
    float   decay_;
    float   reference_;
    float   max_gain_;
    float   gain_;
};



class LfAGC {
public:
    LfAGC(float attack=10.0f, float decay=0.01f, float reference=0.25f, float max_gain=200.0f)
    : attack_(attack), decay_(decay), reference_(reference), max_gain_(max_gain), gain_(1.0f) {}

    void setAttack(float attack) { attack_ = attack; }
    void setDecay(float decay) { decay_ = decay; }
    void setReference(float reference) { reference_ = reference; }
    void setMaxGain(float max_gain) { max_gain_ = max_gain; }

    float gain(void) { return gain_; }

    float adjust(float sample) {
        float sample_adjusted = sample * gain_;

        float level = std::abs(sample_adjusted);
        float error = reference_ - level;

        if (error > 0.0f) {
            // Adjusted signal level is under reference. Increase gain
            gain_ += decay_ * error;
        } else {
            // Adjusted signal level is over reference. Decrease gain
            gain_ += attack_ * error;
        }

        if (gain_ < 0.0f) {
            gain_ = 0.0f;
        } else if (gain_ > max_gain_) {
            gain_ = max_gain_;
        }

        return sample_adjusted;
    }

private:
    float   attack_;
    float   decay_;
    float   reference_;
    float   max_gain_;
    float   gain_;
};

#endif // AGC_HPP
