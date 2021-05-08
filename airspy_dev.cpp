
#include <cassert>
#include <chrono>
#include <iostream>

#include <stdio.h>
#include <inttypes.h>
#include <stdint.h>
#include <libairspy/airspy.h>

#include "airspy_dev.hpp"


// This is not a member function so we only have access to the class public
// members.
static int data_cb(airspy_transfer_t *transfer) {
    AirspyDev &self = *reinterpret_cast<AirspyDev*>(transfer->ctx);

    std::cout << "Device " << self.getSerial() << ": data_cb" << std::endl;

    // Return 0 to continue or != 0 to stop
    return 0;
}


AirspyDev::AirspyDev(const std::string &serial) : dev_(nullptr), serial_(serial), run_(false)  {}

AirspyDev::~AirspyDev(void) {
    assert(dev_ == nullptr);
    assert(run_ == false);
}

int AirspyDev::open(void) {
    int      ret = 0;
    uint64_t serial;

    assert(dev_ == nullptr);

    ret = sscanf(serial_.c_str(), "%" SCNx64, &serial);
    if (ret != 1) return -1;

    ret = airspy_open_sn(&dev_, serial);
    if (ret != AIRSPY_SUCCESS) return -1;

    return ret;
}

int AirspyDev::start() {
    assert(dev_ != nullptr);
    assert(run_ == false);
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return 0;
}

int AirspyDev::stop() {
    assert(dev_ != nullptr);
    assert(run_ == true);
    run_ = false;
    worker_thread_.join();

    return 0;
}

int AirspyDev::close(void) {
    assert(dev_ != nullptr);
    assert(run_ == false);

    airspy_close(dev_);
    dev_ = nullptr;

    return 0;
}

std::vector<std::string> AirspyDev::list(void) {
#define MAX_NUM_DEVICES     32
#define MAX_FWSTR_LEN       255
    int                   ret = 0;
    int                   num_devices = 0;
    uint64_t              serials[MAX_NUM_DEVICES];
    uint32_t             *sample_rates;
    uint32_t              num_samplerates = 0;
    char                  firmware_str[MAX_FWSTR_LEN] = "<unknown fw version>";
    struct airspy_device *dev;
    std::vector<std::string> serial_strings;

    num_devices = airspy_list_devices(serials, MAX_NUM_DEVICES);
    if (num_devices > 0 && num_devices <= MAX_NUM_DEVICES) {
        for (int d = 0; d < num_devices; ++d) {
            ret = airspy_open_sn(&dev, serials[d]);
            if (ret == AIRSPY_SUCCESS) {
                airspy_version_string_read(dev, firmware_str, MAX_FWSTR_LEN);
                airspy_get_samplerates(dev, &num_samplerates, 0);
                sample_rates = (uint32_t*)malloc(num_samplerates * sizeof(uint32_t));
                airspy_get_samplerates(dev, sample_rates, num_samplerates);

                std::ios_base::fmtflags save = std::cout.flags();
                std::cout << "    " << std::hex << std::uppercase << serials[d];
                std::cout.flags(save);
                std::cout << " (" << firmware_str << ". ";
                if (num_samplerates > 0) {
                    for (uint32_t i = 0; i < num_samplerates; ++i) {
                        if (i == 0) {
                            std::cout << ((float)sample_rates[i])/1000000.0f;
                        } else {
                            std::cout << ", " << ((float)sample_rates[i])/1000000.0f;
                        }
                    }
                    std::cout << " MSPS)\n";
                } else {
                    std::cout << "No supported samplerates)\n";
                }

                free(sample_rates);

                airspy_close(dev);
            } else {
                std::cout << "    " << std::hex << std::uppercase << serials[d] << " (in use)" << std::endl;
            }
        }
    }

    return serial_strings;
}

/*
void AirspyDev::data_cb_(airspy_transfer_t *transfer) {
    AirspyDev &self = *reinterpret_cast<AirspyDev*>(transfer->ctx);

    if (!self.run_) return;

    std::cout << "Device " << self.serial_ << ": data_cb" << std::endl;
}
*/


void AirspyDev::worker_(AirspyDev &self) {
    airspy_transfer_t transfer;

    transfer.ctx = &self;

    // Handle disconnect of dongle nicely
    while (self.run_) {
        std::cout << "Device " << self.serial_ << ": working..." << std::endl;
        data_cb(&transfer);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
}
