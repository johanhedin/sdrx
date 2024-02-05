//
// Multi-Stage translating down sampler for tuning and decimating a IQ stream
//
// @author Johan Hedin
// @date   2021 - 2024
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

#if defined __AVX2__
#include <immintrin.h>
#elif defined __ARM_NEON
#include <arm_neon.h>
#else
//#pragma message "NO AVX2 or NEON SIMD available. Using normal code"
#endif

// Multi-Stage Translating Down sampler
class MSD {
public:
    // Configuration for one down sampling stage. We need the down sampling
    // factor and the coefficients that make up the low pass FIR filter
    struct Stage {
        unsigned           m;    // Down sampling factor
        std::vector<float> coef; // Low pass filter FIR coefficients
    };

    MSD(void) = default;

    // Construct a MSD. Argument is a translation vector and a list of stage configurations
    MSD(const std::vector<iqsample_t> &translator, const std::vector<MSD::Stage> &stages, bool use_ftfir = false) :
      m_(1), translator_(translator), trans_pos_(0), use_ftfir_(use_ftfir) {
        auto iter = stages.begin();
        while (iter != stages.end()) {
            if (iter == stages.begin() && !translator.empty()) {
                stages_.push_back(MSD::S(iter->m, iter->coef, translator));
            } else {
                stages_.push_back(MSD::S(iter->m, iter->coef));
            }
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
            if (use_ftfir_) {
                for (unsigned i = 0; i < in_len; ++i) {
                    sample = in[i];
                    auto stage_iter = stages_.begin();
                    if (stage_iter->addSample(sample)) {
                        // The stage produced a out sample
                        sample = stage_iter->calculateOutputTranslated();
                        ++stage_iter;
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
                    } else {
                        // The stage need more samples. Do nothing more
                    }

                    if (stage_iter == stages_.end()) {
                        // Write out sample
                        *(out++) = sample;
                        out_len += 1;
                    }
                }
            } else {
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
        }

        if (out_len_ptr) *out_len_ptr = out_len;
    }

private:
    // Internal class representing one stage with its associated delay line
    // in the form of a ring buffer
    class S {
    public:
        S(unsigned m, const std::vector<float> c, const std::vector<iqsample_t> &translator = std::vector<iqsample_t>(0)) :
          m_(m), c_(c), rb_(c.size() * 2, iqsample_t(0.0f, 0.0f)), pos_(0), isn_(m), ccs_(0) {
            if (!translator.empty()) {
                // Construct frequency translating filter coeffcient set based
                // on m, c and translator vector. Size of translator is assumed
                // to always be evenly divisalbe by m.
                unsigned num_sets = translator.size() / m_;
                unsigned j = 0;
                for (unsigned i = 0; i < num_sets; i++) {
                    std::vector<iqsample_t> cc;
                    auto iter = c_.begin();
                    unsigned k = j;
                    while (iter != c_.end()) {
                        // Frequency translating FIR filter has a gain of 0.5
                        // so we need to compensate that with a factor 2 on
                        // the coefficients
                        cc.push_back(*iter * translator[k] * 2.0f);
                        if (++k == translator.size()) k = 0;
                        ++iter;
                    }

                    cs_.push_back(cc);
                    j += m_;
                }
            }

            // Create a second coefficient vector where every coefficent from
            // the original vector is dubbled. This is used for SIMD if
            // available
            auto iter = c_.begin();
            while (iter != c_.end()) {
                c2_.push_back(iqsample_t(*iter, *iter));
                ++iter;
            }
        }

        // Add one new sample to the delay line
        inline bool addSample(iqsample_t sample) {
            bool ret = false;

            // Add sample to the ring buffer at current position. Ring buffer
            // has doubble length and we add the sample at two positions
            rb_[pos_] = sample;
            rb_[pos_ + c_.size()] = sample;

            // Advance sample pointer. If at the end, wrap around
            if (++pos_ == c_.size()) pos_ = 0;

            // Decrease samples needed. If 0, we have enough new in samples
            // in the ring buffer to calculate one output sample
            if (--isn_ == 0) {
                isn_ = m_;
                ret = true;
            }

            return ret;
        }

        // Calculat one output sample based on the in samples in the delay line
        // and the filter coefficients
        inline iqsample_t calculateOutput(void) {
            unsigned   i = 0;
            auto rb_ptr = &rb_[pos_];
            auto c_ptr  = &c_[0];
            float real_sum = 0.0f, imag_sum = 0.0f;
            unsigned rounded_size = (c_.size() >> 2) << 2;

            /*
            // Simple calculation. Not vectorize friendly. Not used
            iqsample_t out_sample(0.0f, 0.0f);
            for (; i < c_.size(); ++i) {
                out_sample += rb_ptr[i] * c_ptr[i];
            }
            real_sum = out_sample.real();
            imag_sum = out_sample.imag();
            */

            // TODO: We can turn the for loop the other way around and start
            //       at pos_ + c_.size() - 1. Then check for >= 4 without the
            //       need to shift. And then the next loop can unroll by 2
            //       and so on.

            // TODO: Optimize the for loop for folding FIR filter since our
            //       coefficients are symetrical

            // TODO: Convert c_ to an array of complex instead. Store the
            //       same coefficient in both real and imag. This will
            //       streamline the SIMD. Just need to att .real() or .imad()
            //       for the old scalar cases.

            // Vectorize friendly loops. We loop in batches of 4 as long as we
            // can and unroll the calculations. Then we clean up with a final
            // loop of 3, 2 or 1 iteration(s). We need to split the complex
            // multiplications into real and imaginary to trick g++ into
            // vectorization.
#ifdef __AVX2__
            // Intel AVX SIMD variant.
            __m256 vec_sum = _mm256_set1_ps(0.0f);
            for (; i < rounded_size; i += 4) {
                __m256 a = _mm256_loadu_ps((float*)&rb_ptr[i]); // 4 samples is 8 floats (I and Q interleaved)
                __m256 b = _mm256_loadu_ps((float*)&c2_[i]);    // We multiply with our special coefficent vector
                vec_sum = _mm256_fmadd_ps(a, b, vec_sum);
            }

            // Need to complete the summation
            for (auto j = 0; j < 8; j += 2) {
                real_sum += vec_sum[j + 0];
                imag_sum += vec_sum[j + 1];
            }

            for (; i < c_.size(); ++i) {
                real_sum += rb_ptr[i].real() * c_ptr[i];
                imag_sum += rb_ptr[i].imag() * c_ptr[i];
            }
#else
            // Portable variant.
            for (; i < rounded_size; i += 4) {
                real_sum += (rb_ptr[i + 0].real() * c_ptr[i + 0]
                           + rb_ptr[i + 1].real() * c_ptr[i + 1]
                           + rb_ptr[i + 2].real() * c_ptr[i + 2]
                           + rb_ptr[i + 3].real() * c_ptr[i + 3]
                );
                imag_sum += (rb_ptr[i + 0].imag() * c_ptr[i + 0]
                           + rb_ptr[i + 1].imag() * c_ptr[i + 1]
                           + rb_ptr[i + 2].imag() * c_ptr[i + 2]
                           + rb_ptr[i + 3].imag() * c_ptr[i + 3]
                );
            }
            for (; i < c_.size(); ++i) {
                real_sum += rb_ptr[i].real() * c_ptr[i];
                imag_sum += rb_ptr[i].imag() * c_ptr[i];
            }
#endif

            return iqsample_t(real_sum, imag_sum);
        }

        // Calculat one output sample based on the in samples in the delay line
        // and the frequency translating filter coefficient set. This function
        // is called as the first one if ftfir is used. All subsequent calls
        // use the "normal" calculateOutput() above.
        inline iqsample_t calculateOutputTranslated(void) {
            unsigned   i = 0;
            auto rb_ptr = &rb_[pos_];
            auto c_ptr  = &cs_[ccs_][0];
            unsigned rounded_size = (c_.size() >> 2) << 2;
            float real_sum = 0.0f, imag_sum = 0.0f;

            /*
            // Simple calculation. Not vectorize friendly
            iqsample_t out_sample(0.0f, 0.0f);
            for (; i < c_.size(); ++i) {
                out_sample += rb_ptr[i] * c_ptr[i];
            }
            real_sum = out_sample.real();
            imag_sum = out_sample.imag();
            */

            // Vectorize friendly loops. We loop in batches of 4 as long as we
            // can and unroll the calculations. Then we clean up with a final
            // loop of 3, 2 or 1 iteration(s). We need to split the complex
            // multiplications into real and imaginary to trick g++ into
            // vectorization.
#ifdef __AVX2__
            // Intel AVX SIMD variant.
            __m256 sumr = _mm256_set1_ps(0.0f);
            __m256 sumi = _mm256_set1_ps(0.0f);
            const __m256 conj = _mm256_set_ps(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
            for (; i < rounded_size; i += 4) {
                __m256 a = _mm256_loadu_ps((float*)&rb_ptr[i]);
                __m256 b = _mm256_loadu_ps((float*)&c_ptr[i]);

                /*
                // Normal
                __m256 b_conj = _mm256_mul_ps(b, conj);
                __m256 cr = _mm256_mul_ps(a, b_conj);
                __m256 b_flip = _mm256_permute_ps(b, 0b10110001);
                __m256 ci = _mm256_mul_ps(a, b_flip);
                sumr = _mm256_add_ps(sumr, cr);
                sumi = _mm256_add_ps(sumi, ci);
                */

                // Fused multipy-add
                __m256 b_conj = _mm256_mul_ps(b, conj);
                sumr = _mm256_fmadd_ps(a, b_conj, sumr);
                __m256 b_flip = _mm256_permute_ps(b, 0b10110001);
                sumi = _mm256_fmadd_ps(a, b_flip, sumi);
            }

            // Complete the summation
            sumr = _mm256_hadd_ps(sumr, sumr);
            sumr = _mm256_hadd_ps(sumr, sumr);
            sumr = _mm256_add_ps(sumr, _mm256_permute2f128_ps(sumr, sumr, 1));

            sumi = _mm256_hadd_ps(sumi, sumi);
            sumi = _mm256_hadd_ps(sumi, sumi);
            sumi = _mm256_add_ps(sumi, _mm256_permute2f128_ps(sumi, sumi, 1));

            real_sum = sumr[0];
            imag_sum = sumi[0];

            for (; i < c_.size(); ++i) {
                real_sum += ((rb_ptr[i].real() * c_ptr[i].real() - rb_ptr[i].imag() * c_ptr[i].imag()));
                imag_sum += ((rb_ptr[i].real() * c_ptr[i].imag() + rb_ptr[i].imag() * c_ptr[i].real()));
            }
#else
            // Portable variant.
            for (; i < rounded_size; i += 4) {
                real_sum += ((rb_ptr[i + 0].real() * c_ptr[i + 0].real() - rb_ptr[i + 0].imag() * c_ptr[i + 0].imag())
                       + (rb_ptr[i + 1].real() * c_ptr[i + 1].real() - rb_ptr[i + 1].imag() * c_ptr[i + 1].imag())
                       + (rb_ptr[i + 2].real() * c_ptr[i + 2].real() - rb_ptr[i + 2].imag() * c_ptr[i + 2].imag())
                       + (rb_ptr[i + 3].real() * c_ptr[i + 3].real() - rb_ptr[i + 3].imag() * c_ptr[i + 3].imag()));

                imag_sum += ((rb_ptr[i + 0].real() * c_ptr[i + 0].imag() + rb_ptr[i + 0].imag() * c_ptr[i + 0].real())
                       + (rb_ptr[i + 1].real() * c_ptr[i + 1].imag() + rb_ptr[i + 1].imag() * c_ptr[i + 1].real())
                       + (rb_ptr[i + 2].real() * c_ptr[i + 2].imag() + rb_ptr[i + 2].imag() * c_ptr[i + 2].real())
                       + (rb_ptr[i + 3].real() * c_ptr[i + 3].imag() + rb_ptr[i + 3].imag() * c_ptr[i + 3].real()));
            }
            for (; i < c_.size(); ++i) {
                real_sum += ((rb_ptr[i].real() * c_ptr[i].real() - rb_ptr[i].imag() * c_ptr[i].imag()));
                imag_sum += ((rb_ptr[i].real() * c_ptr[i].imag() + rb_ptr[i].imag() * c_ptr[i].real()));
            }
#endif

            // Advance to next coefficient set. Wrap around if necessary
            if (++ccs_ == cs_.size()) ccs_ = 0;

            return iqsample_t(real_sum, imag_sum);
        }

    private:
        unsigned                 m_;     // Decimation factor
        std::vector<float>       c_;     // FIR coefficients
        std::vector<iqsample_t>  c2_;    // FIR coefficients doubbled for SIMD use. Wee cheet with complex
        std::vector<iqsample_t>  rb_;    // Calculation ring buffer
        unsigned                 pos_;   // Current position in the ring buffer
        unsigned                 isn_;   // New in-samples needed before an output sample can be calculated
        std::vector<std::vector<iqsample_t>> cs_; // FT FIR coefficient sets
        unsigned                 ccs_;    // Current coefficient set for FT FIR
    };

    std::vector<MSD::S>     stages_;     // List of stages
    unsigned                m_;          // Total down sampling factor
    std::vector<iqsample_t> translator_; // Frequency tuning sequence
    unsigned                trans_pos_;  // Position in translator
    bool                    use_ftfir_;  // Use frequency translating FIR for firts stage
};

#endif // MSD_HPP
