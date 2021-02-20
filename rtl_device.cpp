
#include <cassert>
#include <chrono>
#include <iostream>

#include <stdio.h>
#include <rtl-sdr.h>

#include "rtl_device.hpp"



RtlDevice::RtlDevice(unsigned id) : dev_(nullptr), id_(id), run_(false) {}

RtlDevice::RtlDevice(const std::string &serial) : dev_(nullptr), id_(0), serial_(serial), run_(false)  {}

RtlDevice::~RtlDevice(void) {
    assert(dev_ == nullptr);
    assert(run_ == false);
}

int RtlDevice::open(void) {
    assert(dev_ == nullptr);

    dev_ = (rtlsdr_dev_t*)&dummy_;

    return 0;
}

int RtlDevice::start() {
    assert(dev_ != nullptr);
    assert(run_ == false);
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return 0;
}

int RtlDevice::stop() {
    assert(dev_ != nullptr);
    assert(run_ == true);
    run_ = false;
    worker_thread_.join();

    return 0;
}

int RtlDevice::close(void) {
    assert(dev_ != nullptr);
    assert(run_ == false);

    dev_ = nullptr;

    return 0;
}

int RtlDevice::list(void) {
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

        std::cout << d << "|" << manufacturer << "|" << product << "|" << serial << std::endl;
    }

    return ret;
}

void RtlDevice::data_cb_(unsigned char *data, uint32_t data_len, void *ctx) {
    RtlDevice &self = *reinterpret_cast<RtlDevice*>(ctx);

    if (!self.run_) return;

    std::cout << "Device " << self.id_ << ": data_cb" << std::endl;
}


void RtlDevice::worker_(RtlDevice &self) {
    // Handle disconnect of dongle nicely
    while (self.run_) {
        std::cout << "Device " << self.id_ << ": working..." << std::endl;
        self.data_cb_(nullptr, 0, &self);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
