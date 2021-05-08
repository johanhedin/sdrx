
#include <cassert>
#include <chrono>
#include <iostream>

#include <stdio.h>

#include <rtl-sdr.h>

#include "rtl_device.hpp"

#define MIN_FQ 45000000
#define MAX_FQ 1700000000

#define MIN_GAIN 0.0f
#define MAX_GAIN 50.0f

#define DEFAULT_FQ          100000000
#define DEFAULT_SAMPLE_RATE 1200000
#define DEFAULT_GAIN        30.0f


RtlDev::RtlDev(unsigned id, int xtal_corr)
: dev_(nullptr), id_(id), xtal_corr_(xtal_corr), fq_(DEFAULT_FQ),
  sample_rate_(DEFAULT_SAMPLE_RATE), gain_(DEFAULT_GAIN), run_(false) {}


RtlDev::RtlDev(const std::string &serial, int xtal_corr)
: dev_(nullptr), id_(0), serial_(serial), xtal_corr_(xtal_corr), fq_(DEFAULT_FQ),
  sample_rate_(DEFAULT_SAMPLE_RATE), gain_(DEFAULT_GAIN), run_(false)  {}


RtlDev::~RtlDev(void) {
    assert(run_ == false);
    assert(dev_ == nullptr);
}


int RtlDev::open(void) {
    int ret = 0;

    assert(dev_ == nullptr);

    if (serial_.length() != 0) {
        int tmp_id = rtlsdr_get_index_by_serial(serial_.c_str());
        if (tmp_id < 0) return -1;

        id_ = (unsigned)tmp_id;
    }

    ret = rtlsdr_open(&dev_, id_);
    if (ret < 0) return -1;

    rtlsdr_set_center_freq(dev_, fq_);
    rtlsdr_set_freq_correction(dev_, xtal_corr_);
    rtlsdr_set_tuner_gain(dev_, (int)(gain_ * 10.0f));
    rtlsdr_set_sample_rate(dev_, sample_rate_);

    return 0;
}


int RtlDev::start() {
    assert(dev_ != nullptr);
    assert(run_ == false);
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return 0;
}


int RtlDev::stop() {
    assert(dev_ != nullptr);
    assert(run_ == true);
    run_ = false;
    worker_thread_.join();

    return 0;
}


int RtlDev::close(void) {
    assert(run_ == false);

    // Device might have bailed out in the worker thread so we just close
    // if device is still open
    if (dev_ != nullptr) {
        rtlsdr_close(dev_);
        dev_ = nullptr;
    }

    return 0;
}


int RtlDev::setFq(uint32_t fq) {
    if (fq < MIN_FQ || fq > MAX_FQ) return -1;

    fq_ = fq;

    if (dev_) {
        rtlsdr_set_center_freq(dev_, fq_);
    }

    return 0;
}


int RtlDev::setSampleRate(uint32_t rate) {
    sample_rate_ = rate;

    if (dev_) {
        rtlsdr_set_sample_rate(dev_, sample_rate_);
    }

    return 0;
}


int RtlDev::setTunerGain(float gain) {
    if (gain < MIN_GAIN || gain > MAX_GAIN) return -1;

    gain_ = gain;

    if (dev_) {
        rtlsdr_set_tuner_gain(dev_, (int)(gain_ * 10.0f));
    }

    return 0;
}


int RtlDev::list(void) {
#define RLT_STR_MAX_LEN 256
    int ret = 0;
    char manufacturer[RLT_STR_MAX_LEN+1];
    char product[RLT_STR_MAX_LEN+1];
    char serial[RLT_STR_MAX_LEN+1];

    uint32_t num_devices = rtlsdr_get_device_count();

    for (uint32_t d = 0; d < num_devices; d++) {
        ret = rtlsdr_get_device_usb_strings(d, manufacturer, product, serial);
        if (ret < 0) {
            std::cerr << "Error: Unable to query USB device strings for device: " << d << std::endl;
            break;
        }

        manufacturer[RLT_STR_MAX_LEN] = 0;
        product[RLT_STR_MAX_LEN] = 0;
        serial[RLT_STR_MAX_LEN] = 0;

        std::cout << "    " << d << ", " << serial << " (" << manufacturer << " " << product << ")" << std::endl;
    }

    return ret;
}


#define RTL_IQ_BUF_SIZE (512 * 75)
void RtlDev::worker_(RtlDev &self) {
    int ret;

    // Handle disconnect of dongle nicely
    while (self.run_) {
        rtlsdr_reset_buffer(self.dev_);
        ret = rtlsdr_read_async(self.dev_, RtlDev::data_cb_, &self, 4, RTL_IQ_BUF_SIZE);
        if (ret < 0) {
            // Try restart...
        }

        //std::cout << "Device " << self.id_ << ": working..." << std::endl;
        //self.data_cb_(nullptr, 0, &self);
        //std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}


void RtlDev::data_cb_(unsigned char *data, uint32_t data_len, void *ctx) {
    RtlDev &self = *reinterpret_cast<RtlDev*>(ctx);

    if (!self.run_) {
        rtlsdr_cancel_async(self.dev_);
        return;
    }

    std::cout << "Device " << self.id_ << ": data_cb" << std::endl;
}
