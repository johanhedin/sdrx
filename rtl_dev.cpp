//
// Device class for a RTL dongle
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

#include <cassert>
#include <chrono>
#include <iostream>
#include <algorithm>

#include <stdio.h>

//#include <rtl-sdr.h>
#include "librtlsdr/include/rtl-sdr.h"

#include "rtl_dev.hpp"

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


static inline std::vector<SampleRate> get_sample_rates(const std::string&) {
    // Sample rates are fixed for a RTL dongle
    std::vector<SampleRate> rates = {
        SampleRate::FS01200,
        SampleRate::FS01440,
        SampleRate::FS01600,
        SampleRate::FS01920,
        SampleRate::FS02400,
        SampleRate::FS02560
    };

    return rates;
}


RtlDev::RtlDev(const std::string &serial, SampleRate fs, int xtal_corr)
: serial_(serial), fs_(fs), xtal_corr_(xtal_corr), fq_(DEFAULT_FQ), gain_(DEFAULT_GAIN),
  lna_gain_idx_(DEFAULT_LNA_GAIN_IDX), mix_gain_idx_(DEFAULT_MIX_GAIN_IDX), vga_gain_idx_(DEFAULT_VGA_GAIN_IDX),
  dev_(nullptr), run_(false), state_(State::IDLE) { }


RtlDev::~RtlDev(void) {
    assert(run_ == false);
}


int RtlDev::start() {
    std::vector<SampleRate> supported_rates;

    if (run_) return RTLDEV_ALREADY_STARTED;

    supported_rates = get_sample_rates(serial_);
    if (std::find(supported_rates.begin(), supported_rates.end(), fs_) == supported_rates.end())
        return RTLDEV_INVALID_SAMPLE_RATE;

    state_ = State::STARTING;
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return RTLDEV_OK;
}


int RtlDev::stop() {
    if (!run_) return RTLDEV_ALREADY_STOPPED;

    run_ = false;
    state_ = State::STOPPING;
    worker_thread_.join();

    return RTLDEV_OK;
}


int RtlDev::setFq(uint32_t fq) {
    int ret;

    if (fq < MIN_FQ || fq > MAX_FQ) return RTLDEV_INVALID_FQ;

    fq_ = fq;

    if (dev_ && state_ == State::RUNNING) {
        ret = rtlsdr_set_center_freq((rtlsdr_dev_t*)dev_, fq_);
        if (ret < 0) return RTLDEV_ERROR;
    }

    return RTLDEV_OK;
}


int RtlDev::setGain(float gain) {
    int ret;
    unsigned lna_gain_idx = 0;
    unsigned mix_gain_idx = 0;
    unsigned vga_gain_idx = 12;
    float    tmp_gain = 0.0f;

    if (gain < MIN_GAIN || gain > MAX_GAIN) return RTLDEV_INVALID_GAIN;

    gain_ = gain;

    for (unsigned i = 0; i < 15; i++) {
        if (tmp_gain >= gain_) break;
        tmp_gain += lna_gain_steps[++lna_gain_idx];

        if (tmp_gain >= gain_) break;
        tmp_gain += mix_gain_steps[++mix_gain_idx];
    }

    lna_gain_idx_ = lna_gain_idx;
    mix_gain_idx_ = mix_gain_idx;
    vga_gain_idx_ = vga_gain_idx;

    std::cout << "gain = " << gain_ << " -> lna = " << lna_gain_idx_ << ", mix = " << mix_gain_idx_ << ", vga = " << vga_gain_idx_ << std::endl;

    if (dev_ && state_ == State::RUNNING) {
        ret = rtlsdr_set_tuner_gain_ext((rtlsdr_dev_t*)dev_, lna_gain_idx_, mix_gain_idx_, vga_gain_idx_);
        if (ret < 0) return RTLDEV_ERROR;

    }

    return RTLDEV_OK;
}

int RtlDev::setLnaGain(unsigned idx) {
    int ret;

    if (idx > 15) return RTLDEV_INVALID_GAIN;

    lna_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = rtlsdr_set_tuner_gain_ext((rtlsdr_dev_t*)dev_, lna_gain_idx_, mix_gain_idx_, vga_gain_idx_);
        if (ret < 0) return RTLDEV_ERROR;
    }

    return RTLDEV_OK;
}

int RtlDev::setMixGain(unsigned idx) {
    int ret;

    if (idx > 15) return RTLDEV_INVALID_GAIN;

    mix_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = rtlsdr_set_tuner_gain_ext((rtlsdr_dev_t*)dev_, lna_gain_idx_, mix_gain_idx_, vga_gain_idx_);
        if (ret < 0) return RTLDEV_ERROR;
    }

    return RTLDEV_OK;
}

int RtlDev::setVgaGain(unsigned idx) {
    int ret;

    if (idx > 15) return RTLDEV_INVALID_GAIN;

    vga_gain_idx_ = idx;

    if (dev_ && state_ == State::RUNNING) {
        ret = rtlsdr_set_tuner_gain_ext((rtlsdr_dev_t*)dev_, lna_gain_idx_, mix_gain_idx_, vga_gain_idx_);
        if (ret < 0) return RTLDEV_ERROR;
    }

    return RTLDEV_OK;
}


void RtlDev::worker_(RtlDev &self) {
//#define RTL_IQ_BUF_SIZE    (512 * 75 *2)
#define RTL_NUM_IQ_BUFFERS (16)   // Use 16 buffers to cycle around
    int ret;
    uint32_t rtl_iq_buff_size;
    uint32_t down_sampling_factor;

    if (sample_rate_to_uint(self.fs_) % 16000) {
        std::cerr << "Error: Requested sample rate " << sample_rate_to_str(self.fs_) << "MS/s is not evenly divisible by 16000\n";
    }

    down_sampling_factor = sample_rate_to_uint(self.fs_) / 16000;

    // We want the buffer to be an integral of 512. Down sampling factor is
    // fs/16kHz which is our target down sampling target. The times 2 is because
    // the rtl buffer holds both i and q.
    // We also want the 31.25Hz callback rate which requires fs/31.25 to be a
    // whole number.
    rtl_iq_buff_size = 512 * down_sampling_factor * 2;

    while (self.run_) {
        ret = self.open_();
        if (ret == RTLDEV_OK) {
            std::cerr << "Device " << self.serial_ << " opended successfully\n";
            rtlsdr_reset_buffer((rtlsdr_dev_t*)self.dev_);
            self.state_ = State::RUNNING;
            rtlsdr_read_async((rtlsdr_dev_t*)self.dev_, RtlDev::data_cb_, &self, RTL_NUM_IQ_BUFFERS, rtl_iq_buff_size);
            rtlsdr_close((rtlsdr_dev_t*)self.dev_);
            self.dev_ = nullptr;
            if (self.run_) {
                std::cerr << "Device " << self.serial_ << " disappeared. Trying to reopen...\n";
                self.state_ = State::RESTARTING;
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    self.state_ = State::IDLE;
}


int RtlDev::open_(void) {
    int      ret = 0;
    uint32_t id = 0;
    uint32_t fs = 0;

    // Empty string mean device 0 (a.k.a the first one)
    if (serial_.length() != 0) {
        int tmp_id = rtlsdr_get_index_by_serial(serial_.c_str());
        if (tmp_id < 0) return RTLDEV_DEVICE_NOT_FOUND;

        id = (uint32_t)tmp_id;
    }

    ret = rtlsdr_open((rtlsdr_dev_t**)&dev_, id);
    if (ret < 0) {
        dev_ = nullptr;
        return RTLDEV_UNABLE_TO_OPEN_DEVICE;
    }

    fs = sample_rate_to_uint(fs_);

    ret = rtlsdr_set_center_freq((rtlsdr_dev_t*)dev_, fq_);
    if (ret < 0) goto error;

    ret = rtlsdr_set_freq_correction((rtlsdr_dev_t*)dev_, xtal_corr_);
    if (ret < 0) goto error;

    //rtlsdr_set_tuner_gain((rtlsdr_dev_t*)dev_, (int)(gain_ * 10.0f));
    ret = rtlsdr_set_tuner_gain_ext((rtlsdr_dev_t*)dev_, lna_gain_idx_, mix_gain_idx_, vga_gain_idx_);
    if (ret < 0) goto error;

    ret = rtlsdr_set_sample_rate((rtlsdr_dev_t*)dev_, fs);
    if (ret < 0) goto error;

    goto ok;

error:
    std::cerr << "Error: Unable to set fq/correction/gain or sample rate.\n";
    rtlsdr_close((rtlsdr_dev_t*)dev_);
    dev_ = nullptr;

    return RTLDEV_ERROR;

ok:

    return RTLDEV_OK;
}


void RtlDev::data_cb_(unsigned char *data, uint32_t data_len, void *ctx) {
    RtlDev   &self = *reinterpret_cast<RtlDev*>(ctx);
    uint32_t  data_pos = 0;
    unsigned  iq_pos = 0;

    // Stopping streaming is requested in the ctrl thread but done here
    if (!self.run_) {
        //self.state_ = State::STOPPING; set in stop()
        rtlsdr_cancel_async((rtlsdr_dev_t*)self.dev_);
        return;
    }

    // Convert RTL packed 8-bit IQ data into our complex float IQ buffer
    // (range -1.0 -> 1.0)
    while (data_pos < data_len) {
        // Convert and save I-part
        self.iq_buffer_[iq_pos].real(((float)data[data_pos])   / 127.5f - 1.0f);

        // Convert and save Q-part
        self.iq_buffer_[iq_pos].imag(((float)data[data_pos+1]) / 127.5f - 1.0f);

        iq_pos++;
        data_pos += 2;
    }

    // Emit data
    self.data(self.iq_buffer_, iq_pos, self.fs_);
}


//
// Static functions below
//

std::string RtlDev::errStr(int ret) {
    switch (ret) {
        case RTLDEV_OK:                    return "Ok";
        case RTLDEV_ERROR:                 return "Error";
        case RTLDEV_DEVICE_NOT_FOUND:      return "Device not found";
        case RTLDEV_UNABLE_TO_OPEN_DEVICE: return "Unable to open device";
        case RTLDEV_INVALID_SAMPLE_RATE:   return "Invalid sample rate";
        case RTLDEV_INVALID_FQ:            return "Invalid frequency";
        case RTLDEV_INVALID_GAIN:          return "Invalid gain";
        default:                           return "Unknown";
    }
}


std::vector<RtlDev::Info> RtlDev::list(void) {
#define RLT_STR_MAX_LEN 256
    int                 ret = 0;
    char                manufacturer[RLT_STR_MAX_LEN+1];
    char                product[RLT_STR_MAX_LEN+1];
    char                serial[RLT_STR_MAX_LEN+1];
    std::vector<Info>   devices;
    rtlsdr_dev_t       *rtl_device = nullptr;
    uint32_t            num_devices;

    num_devices = rtlsdr_get_device_count();
    for (uint32_t d = 0; d < num_devices; d++) {
        ret = rtlsdr_get_device_usb_strings(d, manufacturer, product, serial);
        if (ret < 0) {
            break;
        }

        manufacturer[RLT_STR_MAX_LEN] = 0;
        product[RLT_STR_MAX_LEN] = 0;
        serial[RLT_STR_MAX_LEN] = 0;

        Info info;
        info.serial = serial;
        info.index = (unsigned)d;
        info.available = false;
        info.supported = false;
        info.description = manufacturer;
        info.description += " ";
        info.description += product;

        // We need to open the device to check for xtal fq and tuner model
        ret = rtlsdr_open(&rtl_device, d);
        if (ret == 0) {
            info.available = true;

            // Verify 28.8MHz xtal and R820T(2) tuner
            uint32_t rtl2832_clk_fq = 0;
            uint32_t tuner_clk_fq = 0;
            enum rtlsdr_tuner tuner_type;

            rtlsdr_get_xtal_freq(rtl_device, &rtl2832_clk_fq, &tuner_clk_fq);
            tuner_type = rtlsdr_get_tuner_type(rtl_device);

            std::vector<SampleRate> supported_rates = get_sample_rates(info.serial);
            if (tuner_type == RTLSDR_TUNER_R820T && rtl2832_clk_fq == 28800000) {
                info.supported = true;
                for (auto &rate : supported_rates) {
                    info.sample_rates.push_back(rate);
                }
            }

            rtlsdr_close(rtl_device);
        }

        devices.push_back(info);
    }

    return devices;
}


bool RtlDev::isPresent(const std::string &serial) {
#define RLT_STR_MAX_LEN 256
    uint32_t            num_devices;
    char                serial_str[RLT_STR_MAX_LEN+1];
    int                 ret = 0;

    num_devices = rtlsdr_get_device_count();
    for (uint32_t d = 0; d < num_devices; d++) {
        ret = rtlsdr_get_device_usb_strings(d, nullptr, nullptr, serial_str);
        if (ret < 0) {
            break;
        }

        serial_str[RLT_STR_MAX_LEN] = 0;

        if (serial == serial_str) return true;
    }

    return false;
}
