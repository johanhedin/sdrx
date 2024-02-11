//
// Device class for a Airspy R2 or Mini dongle
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

#include <iostream>
#include <algorithm>
#include <cinttypes>

#if defined __AVX2__
#include <immintrin.h>
#elif defined __ARM_NEON
#include <arm_neon.h>
#else
//#pragma message "NO AVX2 or NEON SIMD available. Using normal code"
#endif

#include "../libairspy/libairspy/src/airspy.h"

#include "airspy_dev.hpp"

#define MIN_FQ 45000000
#define MAX_FQ 1700000000

#define MIN_GAIN 0.0f
#define MAX_GAIN 50.0f

#define DEFAULT_FQ            100000000
#define DEFAULT_GAIN          30.0f
// 9, 8, 12 typically represents 30dB-ish
#define DEFAULT_LNA_GAIN_IDX  9
#define DEFAULT_MIX_GAIN_IDX  8
#define DEFAULT_VGA_GAIN_IDX  12


static inline std::vector<SampleRate> get_sample_rates(const std::string& serial_str) {
#define MAX_FWSTR_LEN 255
    int                     ret;
    uint64_t                serial = 0;
    struct airspy_device   *dev = nullptr;
    char                    firmware_str[MAX_FWSTR_LEN] = "<unknown fw version>";
    uint32_t                num_samplerates = 0;
    uint32_t               *sample_rates = nullptr;
    std::vector<SampleRate> rates;

    // If serial string is set, use it. Otherwise defaults to "first device" (serial == 0)
    if (serial_str.length() != 0) {
        ret = sscanf(serial_str.c_str(), "%" SCNx64, &serial);
        if (ret != 1) return rates;
    }

    ret = airspy_open_sn(&dev, serial);
    if (ret == AIRSPY_SUCCESS) {
        airspy_version_string_read(dev, firmware_str, MAX_FWSTR_LEN);
        std::string tmp_str = firmware_str;

        airspy_get_samplerates(dev, &num_samplerates, 0);
        sample_rates = (uint32_t*)malloc(num_samplerates * sizeof(uint32_t));     // TODO: Change from malloc to new (and delete)
        airspy_get_samplerates(dev, sample_rates, num_samplerates);

        if (num_samplerates > 0) {
            for (uint32_t i = 0; i < num_samplerates; ++i) {
                SampleRate tmp_rate = uint_to_sample_rate(sample_rates[i]);
                if (tmp_rate != SampleRate::UNSPECIFIED) rates.push_back(tmp_rate);
            }

            if (rates.size() > 0) {
                if (tmp_str.rfind("AirSpy MINI", 0) == 0) {
                    // AirSpy Mini supports 10MS/s as alternative Fs
                    rates.push_back(SampleRate::FS10000);
                }

                if (tmp_str.rfind("AirSpy NOS", 0) == 0) {
                    // AirSpy R2 supports 6MS/s as alternative Fs
                    rates.push_back(SampleRate::FS06000);
                }
            }

            std::sort(rates.begin(), rates.end());
        }

        free(sample_rates);
        airspy_close(dev);
    }

    return rates;
}


AirspyDev::AirspyDev(const std::string &serial, SampleRate fs)
: R820Dev(serial, fs), fq_(DEFAULT_FQ), gain_(DEFAULT_GAIN),
  lna_gain_idx_(DEFAULT_LNA_GAIN_IDX), mix_gain_idx_(DEFAULT_MIX_GAIN_IDX), vga_gain_idx_(DEFAULT_VGA_GAIN_IDX),
  dev_(nullptr), part_pos_(0), block_size_(0), iq_pos_(0) {
      if (sample_rate_to_uint(fs_) * 4 % 125) {
          std::cerr << "Error: Requested sample rate " << sample_rate_to_str(fs_) << "MS/s is not evenly divisible by 31.25\n";
      }

      block_size_ = sample_rate_to_uint(fs_) * 4 / 125;
}


int AirspyDev::start() {
    std::vector<SampleRate> supported_rates;

    if (run_) return ReturnValue::ALREADY_STARTED;

    supported_rates = get_sample_rates(serial_);
    if (std::find(supported_rates.begin(), supported_rates.end(), fs_) == supported_rates.end())
        return ReturnValue::INVALID_SAMPLE_RATE;

    block_info_.rate = fs_;
    block_info_.pwr = 0.0f;
    block_info_.ts = std::chrono::system_clock::now();
    block_info_.stream_state = StreamState::IDLE;

    state_ = State::STARTING;
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return ReturnValue::OK;
}


int AirspyDev::stop() {
    if (!run_) return ReturnValue::ALREADY_STOPPED;

    run_ = false;
    state_ = State::STOPPING;
    worker_thread_.join();

    return ReturnValue::OK;
}


int AirspyDev::setFq(uint32_t fq) {
    int ret;

    if (fq < MIN_FQ || fq > MAX_FQ) return ReturnValue::INVALID_FQ;

    fq_ = fq;

    if (dev_ && state_ == State::RUNNING) {
        ret = airspy_set_freq((struct airspy_device*)dev_, fq_);
        if (ret != AIRSPY_SUCCESS) return ReturnValue::ERROR;
    }

    return ReturnValue::OK;
}


int AirspyDev::setGain(float gain) {
    int ret;
    unsigned lna_gain_idx = 0;
    unsigned mix_gain_idx = 0;
    unsigned vga_gain_idx = 12;
    float    tmp_gain = 0.0f;

    if (gain < MIN_GAIN || gain > MAX_GAIN) return ReturnValue::INVALID_GAIN;

    gain_ = gain;

    for (unsigned i = 0; i < 15; i++) {
        if (tmp_gain >= gain) break;
        tmp_gain += lna_gain_steps[++lna_gain_idx];

        if (tmp_gain >= gain) break;
        tmp_gain += mix_gain_steps[++mix_gain_idx];
    }

    lna_gain_idx_ = lna_gain_idx;
    mix_gain_idx_ = mix_gain_idx;
    vga_gain_idx_ = vga_gain_idx;

    ret = setLnaGain(lna_gain_idx_);
    if (ret != ReturnValue::OK) return ret;

    ret = setMixGain(mix_gain_idx_);
    if (ret != ReturnValue::OK) return ret;

    ret = setVgaGain(vga_gain_idx_);
    if (ret != ReturnValue::OK) return ret;

    return ReturnValue::OK;
}

int AirspyDev::setLnaGain(unsigned idx) {
    int ret;

    if (idx > 15) return ReturnValue::INVALID_GAIN;

    lna_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = airspy_set_lna_gain((struct airspy_device*)dev_, (uint8_t)lna_gain_idx_);
        if (ret != AIRSPY_SUCCESS) return ReturnValue::ERROR;
    }

    return ReturnValue::OK;
}

int AirspyDev::setMixGain(unsigned idx) {
    int ret;

    if (idx > 15) return ReturnValue::INVALID_GAIN;

    mix_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = airspy_set_mixer_gain((struct airspy_device*)dev_, (uint8_t)mix_gain_idx_);
        if (ret != AIRSPY_SUCCESS) return ReturnValue::ERROR;
    }

    return ReturnValue::OK;
}

int AirspyDev::setVgaGain(unsigned idx) {
    int ret;

    if (idx > 15) return ReturnValue::INVALID_GAIN;

    vga_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = airspy_set_vga_gain((struct airspy_device*)dev_, (uint8_t)vga_gain_idx_);
        if (ret != AIRSPY_SUCCESS) return ReturnValue::ERROR;
    }

    return ReturnValue::OK;
}


void AirspyDev::worker_(AirspyDev &self) {
    int ret;

    while (self.run_) {
        ret = self.open_();
        if (ret == ReturnValue::OK) {
            std::cerr << "Device " << self.serial_ << " opended successfully\n";

            self.block_info_.stream_state = StreamState::STREAMING;
            ret = airspy_start_rx((struct airspy_device*)self.dev_,
                                  reinterpret_cast<airspy_sample_block_cb_fn>(AirspyDev::data_cb_),
                                  &self);
            if (ret == AIRSPY_SUCCESS) {
                self.state_ = State::RUNNING;  // TODO: Move to above airspy_start_rx? How to revert on error then?
                while (self.run_ && airspy_is_streaming((struct airspy_device*)self.dev_) == AIRSPY_TRUE) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                airspy_stop_rx((struct airspy_device*)self.dev_);

                // Send a last data callback to indicate that we have stopped
                // streaming
                self.block_info_.stream_state = StreamState::IDLE;
                self.block_info_.ts = std::chrono::system_clock::now();
                self.data(self.iq_buffer_, 0, self.user_data_, self.block_info_);

                if (self.run_) {
                    self.state_ = State::RESTARTING;
                    std::cerr << "Device " << self.serial_ << " disappeared. Trying to reopen...\n";
                }
            }

            self.block_info_.stream_state = StreamState::IDLE;

            airspy_close((struct airspy_device*)self.dev_);
            self.dev_ = nullptr;
            if (self.run_) std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    self.state_ = State::IDLE;
}


int AirspyDev::open_(void) {
#define PACKING_ON  1
#define PACKING_OFF 0
    int      ret;
    uint64_t serial = 0;

    // If serial string is set, use it. Otherwise defaults to "first device" (serial == 0)
    if (serial_.length() != 0) {
        ret = sscanf(serial_.c_str(), "%" SCNx64, &serial);
        if (ret != 1) return ReturnValue::INVALID_SERIAL;
    }

    ret = airspy_open_sn((struct airspy_device**)&dev_, serial);
    if (ret != AIRSPY_SUCCESS) return ReturnValue::UNABLE_TO_OPEN_DEVICE;

    ret = airspy_set_sample_type((struct airspy_device*)dev_, AIRSPY_SAMPLE_FLOAT32_IQ);
    //ret = airspy_set_sample_type((struct airspy_device*)dev_, AIRSPY_SAMPLE_RAW);
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_packing((struct airspy_device*)dev_, PACKING_ON);
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_samplerate((struct airspy_device*)dev_, sample_rate_to_uint(fs_));
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_freq((struct airspy_device*)dev_, fq_);
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_lna_gain((struct airspy_device*)dev_, (uint8_t)lna_gain_idx_);
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_mixer_gain((struct airspy_device*)dev_, (uint8_t)mix_gain_idx_);
    if (ret != AIRSPY_SUCCESS) goto error;

    ret = airspy_set_vga_gain((struct airspy_device*)dev_, (uint8_t)vga_gain_idx_);
    if (ret != AIRSPY_SUCCESS) goto error;

    return ReturnValue::OK;

error:
    airspy_close((struct airspy_device*)dev_);
    dev_ = nullptr;

    return ReturnValue::ERROR;
}


int AirspyDev::data_cb_(void *t) {
    airspy_transfer_t *transfer = reinterpret_cast<airspy_transfer_t*>(t);
    AirspyDev &self = *reinterpret_cast<AirspyDev*>(transfer->ctx);
    int sample_pos = 0;

    self.block_info_.ts = std::chrono::system_clock::now();

    if (!self.run_) {
        return 0;
    }

    if (transfer->dropped_samples) {
        std::cerr << "Warning: " << transfer->dropped_samples << " samples dropped. Your system is probably overloaded.\n";
    }

    sample_pos = 0;
    float *data = (float*)transfer->samples;
    for (int i = 0; i < transfer->sample_count; i++) {
        self.iq_buffer_[self.part_pos_ + self.iq_pos_] = iqsample_t(data[sample_pos], data[sample_pos+1]);
        sample_pos += 2;

        self.iq_pos_++;
        if (self.iq_pos_ == self.block_size_) {
            // IQ buffer ready to be dispatched. Calculate RMS and emit data

            // TODO: Need to compensate self.block_info_.ts for half use of buffer.

            float pwr_rms = 0.0f;

            // Calculate average power in the chunk by squaring the amplitude RMS.
            // ampl_rms = sqrt( ( sum( abs(iq_sample)^2 ) ) / N )
#ifdef __AVX2__
            // Intel AVX SIMD variant.
            unsigned rounded_size = (self.block_size_ >> 2) << 2;
            unsigned i = 0;
            iqsample_t *sample_ptr = &self.iq_buffer_[self.part_pos_];
            __m256 sum = _mm256_set1_ps(0.0f);
            for (; i < rounded_size; i += 4) {
                __m256 a = _mm256_loadu_ps((float*)&sample_ptr[i]);

                // Fused multipy-add
                sum = _mm256_fmadd_ps(a, a, sum);
            }

            // Complete the summation
            sum = _mm256_hadd_ps(sum, sum);
            sum = _mm256_hadd_ps(sum, sum);
            sum = _mm256_add_ps(sum, _mm256_permute2f128_ps(sum, sum, 1));

            pwr_rms = sum[0];

            for (; i < self.block_size_; ++i) {
                pwr_rms += std::norm(sample_ptr[i]);
            }
            pwr_rms = pwr_rms / self.block_size_;
#else
            // Portable variant.
            for (unsigned i = 0; i < self.block_size_; i++) {
                float ampl_squared = std::norm(self.iq_buffer_[self.part_pos_ + i]);
                pwr_rms += ampl_squared;
            }
            pwr_rms = pwr_rms / self.block_size_;
#endif

            // Calculate power dBFS with full scale sine wave as reference (amplitude
            // of 1/sqrt(2) or power 1/2 or -3 dB).
            self.block_info_.pwr = 10 * std::log10(pwr_rms) - 3.0f;

            self.data(&self.iq_buffer_[self.part_pos_], self.block_size_, self.user_data_, self.block_info_);


            if (self.part_pos_ == 0) {
                self.part_pos_ = self.block_size_;
            } else {
                self.part_pos_ = 0;
            }

            self.iq_pos_ = 0;
        }
    }

    // Return 0 to continue or != 0 to stop
    return 0;
}


//
// Static functions below
//

std::vector<R820Dev::Info> AirspyDev::list(void) {
#define MAX_NUM_DEVICES   32
#define MAX_FWSTR_LEN     255
#define MAX_SERSTR_LEN    255
    int                   ret = 0;
    int                   num_devices = 0;
    uint64_t              serials[MAX_NUM_DEVICES];
    uint32_t             *sample_rates;
    uint32_t              num_samplerates = 0;
    char                  firmware_str[MAX_FWSTR_LEN] = "<unknown fw version>";
    char                  serial_str[MAX_SERSTR_LEN];
    struct airspy_device *dev;
    std::vector<Info>     devices;

    num_devices = airspy_list_devices(serials, MAX_NUM_DEVICES);
    if (num_devices > 0 && num_devices <= MAX_NUM_DEVICES) {
        for (int d = 0; d < num_devices; ++d) {
            snprintf(serial_str, MAX_SERSTR_LEN, "%.16" PRIX64, serials[d]);

            Info info;
            info.type = Type::AIRSPY;
            info.serial = serial_str;
            info.index = (unsigned)d;
            info.available = false;
            info.supported = false;

            // We need to open the device to get sample rates
            ret = airspy_open_sn(&dev, serials[d]);
            if (ret == AIRSPY_SUCCESS) {
                info.available = true;
                airspy_version_string_read(dev, firmware_str, MAX_FWSTR_LEN);
                info.description = firmware_str;

                airspy_get_samplerates(dev, &num_samplerates, 0);
                sample_rates = (uint32_t*)malloc(num_samplerates * sizeof(uint32_t));
                airspy_get_samplerates(dev, sample_rates, num_samplerates);

                if (num_samplerates > 0) {
                    for (uint32_t i = 0; i < num_samplerates; ++i) {
                        SampleRate tmp_rate = uint_to_sample_rate(sample_rates[i]);
                        if (tmp_rate != SampleRate::UNSPECIFIED)
                            info.sample_rates.push_back(tmp_rate);
                    }

                    if (info.sample_rates.size() > 0) {
                        info.supported = true;

                        if (info.description.rfind("AirSpy MINI", 0) == 0) {
                            // AirSpy Mini supports 10MS/s as alternative Fs
                            info.sample_rates.push_back(SampleRate::FS10000);
                        }

                        if (info.description.rfind("AirSpy NOS", 0) == 0) {
                            // AirSpy R2 supports 6MS/s as alternative Fs
                            info.sample_rates.push_back(SampleRate::FS06000);
                        }

                        info.default_sample_rate = SampleRate::FS06000;
                    }

                    std::sort(info.sample_rates.begin(), info.sample_rates.end());
                }

                free(sample_rates);
                airspy_close(dev);
            }

            devices.push_back(info);
        }
    }

    return devices;
}


bool AirspyDev::isPresent(const std::string &serial) {
#define MAX_NUM_DEVICES   32
#define MAX_SERSTR_LEN    255
    int                   num_devices = 0;
    uint64_t              serials[MAX_NUM_DEVICES];
    char                  serial_str[MAX_SERSTR_LEN];

    num_devices = airspy_list_devices(serials, MAX_NUM_DEVICES);
    if (num_devices > 0 && num_devices <= MAX_NUM_DEVICES) {
        for (int d = 0; d < num_devices; ++d) {

            snprintf(serial_str, MAX_SERSTR_LEN, "%.16" PRIX64, serials[d]);

            if (serial == serial_str) return true;
        }
    }

    return false;
}


bool AirspyDev::rateSupported(const std::string &serial, SampleRate rate) {
    bool supported = false;

    std::vector<SampleRate> supported_rates = get_sample_rates(serial);
    if (std::find(supported_rates.begin(), supported_rates.end(), rate) != supported_rates.end()) {
        supported = true;
    }

    return supported;
}
