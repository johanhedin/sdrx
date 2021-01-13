
#include <cassert>
#include <chrono>
#include <iostream>

#include <rtl-sdr.h>

#include "rtl_device.hpp"



RtlDevice::RtlDevice(unsigned id) : dev_(nullptr), id_(id), run_(false) {}

RtlDevice::RtlDevice(const std::string serial) : dev_(nullptr), id_(0), run_(false)  {}

RtlDevice::~RtlDevice(void) {
    // Not calling stop() is totally wrong usage
    assert(!run_);
}

int RtlDevice::open(void) {
    return 0;
}

int RtlDevice::start() {
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return 0;
}

int RtlDevice::stop() {
    run_ = false;
    worker_thread_.join();

    return 0;
}

int RtlDevice::close(void) {
    return 0;
}

void RtlDevice::data_cb_(unsigned char *data, uint32_t data_len, void *ctx) {
    RtlDevice *self = reinterpret_cast<RtlDevice*>(ctx);

    if (!self->run_) return;

    std::cout << "Device " << self->id_ << ": data_cb" << std::endl;
}


void RtlDevice::worker_(RtlDevice &self) {
    while (self.run_) {
        std::cout << "Device " << self.id_ << ": working..." << std::endl;
        self.data_cb_(nullptr, 0, &self);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
