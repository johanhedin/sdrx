//
// Multi-Stage translating down sampler for tuning and decimating a IQ stream
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

#ifndef MSD_HPP
#define MSD_HPP

#include <vector>
#include <iqsample.hpp>

// Multi-Stage Down sampler
class MSD {
public:
    // Configuration for one stage. We need to know the decimation factor and
    // the coefficients that make up the FIR low pass filter
    struct Stage {
        unsigned           m;    // Decimation factor
        std::vector<float> coef; // Low pass filter FIR coefficients
    };

    MSD(void) = default;

    // Construct a MSD. Argument is a translation vector and a list of stage configurations
    MSD(const std::vector<iqsample_t> &translator, const std::vector<MSD::Stage> &stages) :
      m_(1), translator_(translator), trans_pos_(0) {
        auto iter = stages.begin();
        while (iter != stages.end()) {
            stages_.push_back(MSD::S(iter->m, iter->coef));
            m_ = m_ * iter->m;
            ++iter;
        }
    }

    // Get decimation factor for the MSD
    unsigned m(void) { return m_; }

    // Translate and down sample. If in_len is a multiple of m, you don't need
    // out_len since you know how many out samples that are to be output.
    inline void decimate(const iqsample_t *in, unsigned in_len, iqsample_t *out, unsigned *out_len_ptr = nullptr) {
        iqsample_t sample;
        unsigned   out_len = 0;

        if (translator_.empty()) {
            // No tuning required
            for (unsigned i = 0; i < in_len; ++i) {
                sample = in[i];

                auto stage_iter = stages_.begin();
                while (stage_iter != stages_.end()) {
                    if (stage_iter->addSample(sample)) {
                        // The stage produced a out sample
                        sample = stage_iter->calculateOutput();
                    } else {
                        // The stage need more samples
                        break;
                    }
                    ++stage_iter;
                }

                if (stage_iter == stages_.end()) {
                    // Write out sample
                    *(out++) = sample;
                    out_len += 1;
                }
            }
        } else {
            // Tune to requested channel
            for (unsigned i = 0; i < in_len; ++i) {
                sample = in[i] * translator_[trans_pos_];
                if (++trans_pos_ == translator_.size()) trans_pos_ = 0;

                auto stage_iter = stages_.begin();
                while (stage_iter != stages_.end()) {
                    if (stage_iter->addSample(sample)) {
                        // The stage produced a out sample
                        sample = stage_iter->calculateOutput();
                    } else {
                        // The stage need more samples
                        break;
                    }
                    ++stage_iter;
                }

                if (stage_iter == stages_.end()) {
                    // Write out sample
                    *(out++) = sample;
                    out_len += 1;
                }
            }
        }

        if (out_len_ptr) *out_len_ptr = out_len;
    }

private:
    // Internal class representing one stage with its associated delay line
    // in the form of a ring buffer
    class S {
    public:
        S(unsigned m, const std::vector<float> c) : m_(m), c_(c),
            rb_(c.size(), iqsample_t(0.0f, 0.0f)), pos_(0), isn_(m) {}

        // Add one new sample to the delay line
        inline bool addSample(iqsample_t sample) {
            bool ret = false;

            // Add sample to the ring buffer at current position
            rb_[pos_] = sample;

            // Advance and if at the end, wrap around
            if (++pos_ == rb_.size()) pos_ = 0;

            // Decrease samples needed. If 0, we have enough new in samples
            // to calculate one output sample
            if (--isn_ == 0) {
                isn_ = m_;
                ret = true;
            }

            return ret;
        }

        // Calculat one output sample based on the in samples in the delay line
        // and the filter coefficients
        inline iqsample_t calculateOutput(void) {
            auto rb_pos = pos_;

            iqsample_t out_sample(0.0f, 0.0f);
            for (unsigned i = 0; i < c_.size(); ++i) {
                out_sample += rb_[rb_pos] * c_[i];
                if (++rb_pos == rb_.size()) rb_pos = 0;
            }

            return out_sample;
        }

    private:
        unsigned                 m_;     // Decimation factor
        std::vector<float>       c_;     // FIR coefficients
        std::vector<iqsample_t>  rb_;    // Calculation ring buffer
        unsigned                 pos_;   // Current position in the ring buffer
        unsigned                 isn_;   // New in-samples needed before an output sample can be calculated
    };

    std::vector<MSD::S>     stages_;     // List of stages
    unsigned                m_;          // Total decimation factor
    std::vector<iqsample_t> translator_; // Fq tuning sequence
    unsigned                trans_pos_;  // Position in translator
};

#endif // MSD_HPP
