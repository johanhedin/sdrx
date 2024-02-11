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
#include <cassert>
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
        std::vector<float> h;    // Low pass filter FIR coefficients
    };

    MSD(void) = default;

    // Construct a MSD. Argument is a translation vector and a list of stage configurations
    MSD(const std::vector<iqsample_t> &translator, const std::vector<MSD::Stage> &stages, bool use_ftfir = false) :
      m_(1), translator_(translator), trans_pos_(0), use_ftfir_(use_ftfir) {
        auto iter = stages.begin();
        while (iter != stages.end()) {
            if (iter == stages.begin() && !translator.empty()) {
                stages_.push_back(MSD::S(iter->m, iter->h, translator));
            } else {
                stages_.push_back(MSD::S(iter->m, iter->h));
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
    // Internal class representing one stage. Contains the delay line and
    // filter coefficients
    class S {
    public:
        S(unsigned m, const std::vector<float> h, const std::vector<iqsample_t> &translator = std::vector<iqsample_t>(0)) :
          m_(m), d_(h.size() * 2, iqsample_t(0.0f, 0.0f)), pos_(0), isn_(m), k_(0) {
            // Make sure that the filter coefficients can be used for folded FIR
            assert(h.size() > 0);
            assert((h.size() - 1) % 2 == 0);

            // Make sure the translator size and down sampling factor supports
            // frequency translating FIR
            assert(translator.size() % m_ == 0);

            // We store our filter coefficients in a complex vector with
            // the same value in the real and imaginary parts. This is then
            // used in the SIMD optimized code to allow easy loding into
            // SIMD registers. When we just need "real" coefficients, we
            // can read the value from the real or imag part since it
            // is the same.
            auto iter = h.begin();
            while (iter != h.end()) {
                h_.push_back(iqsample_t(*iter, *iter));
                ++iter;
            }

            if (translator.size() > 0) {
                // Construct frequency translating filter coefficient set based
                // on m, h and translator vector. Size of translator is assumed
                // to always be evenly divisalbe by m.
                unsigned num_sets = translator.size() / m_;
                unsigned j = 0;
                for (unsigned set = 0; set < num_sets; set++) {
                    std::vector<iqsample_t> cc;
                    auto iter = h.begin();
                    unsigned k = j;
                    while (iter != h.end()) {
                        // Frequency translating FIR filter has a gain of 0.5
                        // so we need to compensate that with a factor 2 on
                        // the coefficients
                        cc.push_back(*iter * translator[k] * 2.0f);
                        if (++k == translator.size()) k = 0;
                        ++iter;
                    }

                    hk_.push_back(cc);
                    j += m_;
                }
            }
        }

        // Add one new sample to the delay line
        inline bool addSample(iqsample_t sample) {
            bool ret = false;

            // Add sample to the delay line at current position. Delay line
            // has doubble length and we also add the sample to pos + size
            d_[pos_] = sample;
            d_[pos_ + h_.size()] = sample;

            // Advance delay line write pointer. If at the end, wrap around
            if (++pos_ == h_.size()) pos_ = 0;

            // Decrease samples needed. If 0, we have enough new samples in
            // the delay line to calculate one output sample
            if (--isn_ == 0) {
                isn_ = m_;
                ret = true;
            }

            return ret;
        }

        // Calculate one output sample based on the samples in the delay line
        // and the filter coefficients
        inline iqsample_t calculateOutput(void) {
            unsigned half_h_size = (h_.size() - 1) >> 1;
            unsigned rounded_half_h_size = (half_h_size >> 2) << 2;
            unsigned i = 0;
            unsigned j = h_.size();
            float    real_sum = 0.0f;
            float    imag_sum = 0.0f;
            auto     d_ptr = &d_[pos_];
            auto     h_ptr = &h_[0];

            /*
            // Simple calculation. Not vectorize friendly. Not used
            for (; i < h_.size(); ++i) {
                real_sum += d_ptr[i].real() * h_ptr[i].real();
                imag_sum += d_ptr[i].imag() * h_ptr[i].imag();
            }
            */

            // Vectorize friendly loops. We loop in batches of 4 as long as we
            // can and unroll the calculations. Then we clean up with a final
            // loop of 3, 2 or 1 iteration(s). We split the complex multiplications
            // into real and imaginary parts ourselves to trick g++ into some
            // vectorization for the portable version.
#ifdef __AVX2__
            // Intel AVX SIMD variant of a folded FIR
            __m256 vec_sum = _mm256_set1_ps(0.0f);
            for (; i < rounded_half_h_size; i += 4, j -= 4) {
                __m256 vec_h    = _mm256_loadu_ps((float*)&h_ptr[i]);
                __m256 vec_d    = _mm256_loadu_ps((float*)&d_ptr[i]);
                __m256 vec_d_hi = _mm256_loadu_ps((float*)&d_ptr[j - 4]);

                // vec_d_hi need to be reversed
                vec_d_hi = _mm256_permute_ps(vec_d_hi, 0b01001110);
                vec_d_hi = _mm256_permute2f128_ps(vec_d_hi, vec_d_hi, 1);

                // Sum vec_d with reversed vec_d_hi
                vec_d = _mm256_add_ps(vec_d, vec_d_hi);

                // Fused multipy-add
                vec_sum = _mm256_fmadd_ps(vec_h, vec_d, vec_sum);
            }
            // Complete the vector summation
            for (auto q = 0; q < 8; q += 2) {
                real_sum += vec_sum[q + 0];
                imag_sum += vec_sum[q + 1];
            }
            --j;

            // Clean up the trailing 3, 2 or 1 samples with normal code
            for (; i < half_h_size; ++i, --j) {
                real_sum += (d_ptr[i].real() + d_ptr[j].real()) * h_ptr[i].real();
                imag_sum += (d_ptr[i].imag() + d_ptr[j].imag()) * h_ptr[i].imag();
            }

            // Finalize by adding the "center" sample * coefficient
            real_sum += d_ptr[i].real() * h_ptr[i].real();
            imag_sum += d_ptr[i].imag() * h_ptr[i].imag();
#else
            // Portable version of a folded FIR
            --j;
            for (; i < rounded_half_h_size; i += 4, j -= 4) {
                real_sum += (d_ptr[i + 0].real() + d_ptr[j - 0].real()) * h_ptr[i + 0].real();
                real_sum += (d_ptr[i + 1].real() + d_ptr[j - 1].real()) * h_ptr[i + 1].real();
                real_sum += (d_ptr[i + 2].real() + d_ptr[j - 2].real()) * h_ptr[i + 2].real();
                real_sum += (d_ptr[i + 3].real() + d_ptr[j - 3].real()) * h_ptr[i + 3].real();

                imag_sum += (d_ptr[i + 0].imag() + d_ptr[j - 0].imag()) * h_ptr[i + 0].imag();
                imag_sum += (d_ptr[i + 1].imag() + d_ptr[j - 1].imag()) * h_ptr[i + 1].imag();
                imag_sum += (d_ptr[i + 2].imag() + d_ptr[j - 2].imag()) * h_ptr[i + 2].imag();
                imag_sum += (d_ptr[i + 3].imag() + d_ptr[j - 3].imag()) * h_ptr[i + 3].imag();
            }

            // Clean up the trailing 3, 2 or 1 samples
            for (; i < half_h_size; ++i, --j) {
                real_sum += (d_ptr[i].real() + d_ptr[j].real()) * h_ptr[i].real();
                imag_sum += (d_ptr[i].imag() + d_ptr[j].imag()) * h_ptr[i].imag();
            }

            // Finalize by adding the "center" sample * coefficient
            real_sum += d_ptr[i].real() * h_ptr[i].real();
            imag_sum += d_ptr[i].imag() * h_ptr[i].imag();
#endif

            return iqsample_t(real_sum, imag_sum);
        }

        // Calculate one output sample based on the samples in the delay line
        // and the frequency translating filter coefficient sets. This function
        // is called as the first one if ftfir is used. All subsequent calls
        // use the "normal" calculateOutput() above.
        inline iqsample_t calculateOutputTranslated(void) {
            unsigned rounded_h_size = (h_.size() >> 2) << 2;
            unsigned i = 0;
            float real_sum = 0.0f;
            float imag_sum = 0.0f;
            auto d_ptr = &d_[pos_];
            auto h_ptr = &hk_[k_][0];

            /*
            // Simple calculation. Not vectorize friendly
            iqsample_t out_sample(0.0f, 0.0f);
            for (; i < h_.size(); ++i) {
                out_sample += d_ptr[i] * h_ptr[i];
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
            __m256 real_sum_vec = _mm256_set1_ps(0.0f);
            __m256 imag_sum_vec = _mm256_set1_ps(0.0f);
            const __m256 neg_vec = _mm256_set_ps(-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f);
            for (; i < rounded_h_size; i += 4) {
                __m256 d_vec = _mm256_loadu_ps((float*)&d_ptr[i]);
                __m256 h_vec = _mm256_loadu_ps((float*)&h_ptr[i]);

                // Fused multipy-add
                __m256 h_vec_neg = _mm256_mul_ps(h_vec, neg_vec);
                real_sum_vec = _mm256_fmadd_ps(d_vec, h_vec_neg, real_sum_vec);

                __m256 h_vec_perm = _mm256_permute_ps(h_vec, 0b10110001);
                imag_sum_vec = _mm256_fmadd_ps(d_vec, h_vec_perm, imag_sum_vec);
            }

            // Complete the summation
            real_sum_vec = _mm256_hadd_ps(real_sum_vec, real_sum_vec);
            real_sum_vec = _mm256_hadd_ps(real_sum_vec, real_sum_vec);
            real_sum_vec = _mm256_add_ps(real_sum_vec, _mm256_permute2f128_ps(real_sum_vec, real_sum_vec, 1));

            imag_sum_vec = _mm256_hadd_ps(imag_sum_vec, imag_sum_vec);
            imag_sum_vec = _mm256_hadd_ps(imag_sum_vec, imag_sum_vec);
            imag_sum_vec = _mm256_add_ps(imag_sum_vec, _mm256_permute2f128_ps(imag_sum_vec, imag_sum_vec, 1));

            real_sum = real_sum_vec[0];
            imag_sum = imag_sum_vec[0];

            // Clean up the trailing 3, 2 or 1 samples with normal code
            for (; i < h_.size(); ++i) {
                real_sum += (d_ptr[i].real() * h_ptr[i].real() - d_ptr[i].imag() * h_ptr[i].imag());
                imag_sum += (d_ptr[i].real() * h_ptr[i].imag() + d_ptr[i].imag() * h_ptr[i].real());
            }
#else
            // Portable variant.
            for (; i < rounded_h_size; i += 4) {
                real_sum += (d_ptr[i + 0].real() * h_ptr[i + 0].real() - d_ptr[i + 0].imag() * h_ptr[i + 0].imag());
                real_sum += (d_ptr[i + 1].real() * h_ptr[i + 1].real() - d_ptr[i + 1].imag() * h_ptr[i + 1].imag());
                real_sum += (d_ptr[i + 2].real() * h_ptr[i + 2].real() - d_ptr[i + 2].imag() * h_ptr[i + 2].imag());
                real_sum += (d_ptr[i + 3].real() * h_ptr[i + 3].real() - d_ptr[i + 3].imag() * h_ptr[i + 3].imag());

                imag_sum += (d_ptr[i + 0].real() * h_ptr[i + 0].imag() + d_ptr[i + 0].imag() * h_ptr[i + 0].real());
                imag_sum += (d_ptr[i + 1].real() * h_ptr[i + 1].imag() + d_ptr[i + 1].imag() * h_ptr[i + 1].real());
                imag_sum += (d_ptr[i + 2].real() * h_ptr[i + 2].imag() + d_ptr[i + 2].imag() * h_ptr[i + 2].real());
                imag_sum += (d_ptr[i + 3].real() * h_ptr[i + 3].imag() + d_ptr[i + 3].imag() * h_ptr[i + 3].real());
            }

            // Clean up the trailing 3, 2 or 1 samples
            for (; i < h_.size(); ++i) {
                real_sum += (d_ptr[i].real() * h_ptr[i].real() - d_ptr[i].imag() * h_ptr[i].imag());
                imag_sum += (d_ptr[i].real() * h_ptr[i].imag() + d_ptr[i].imag() * h_ptr[i].real());
            }
#endif

            // Advance to next coefficient set. Wrap around if necessary
            if (++k_ == hk_.size()) k_ = 0;

            return iqsample_t(real_sum, imag_sum);
        }

    private:
        using hk_t = std::vector<std::vector<iqsample_t>>; // Vector of FIR filter coefficients, hk

        unsigned                m_;     // Downsampling factor M
        std::vector<iqsample_t> h_;     // FIR coefficients, h, in complex form (same value in real and imag)
        std::vector<iqsample_t> d_;     // Delay line with input samples. 2 * h_.size() to avoid wrap around
        unsigned                pos_;   // Current write position in the delay line
        unsigned                isn_;   // New in-samples needed in the delay line before an output sample can be calculated
        hk_t                    hk_;    // Frequency translating FIR filter coefficient sets
        unsigned                k_;     // Current frequency translating FIR filter coefficient set
    };

    std::vector<MSD::S>     stages_;     // List of stages
    unsigned                m_;          // Total down sampling factor
    std::vector<iqsample_t> translator_; // Frequency tuning sequence
    unsigned                trans_pos_;  // Position in translator
    bool                    use_ftfir_;  // Use frequency translating FIR for firts stage
};

#endif // MSD_HPP
