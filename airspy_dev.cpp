//
// Device class for a Airspy R2 or Mini dongle
//
// @author Johan Hedin
// @date   2021-05-23
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
#include <inttypes.h>
#include <stdint.h>

#include <airspy.h>

#include "airspy_dev.hpp"

#define MIN_FQ 45000000
#define MAX_FQ 1700000000

#define MIN_GAIN 0.0f
#define MAX_GAIN 50.0f

#define DEFAULT_FQ          100000000
#define DEFAULT_GAIN        30.0f


AirspyDev::AirspyDev(const std::string &serial, uint32_t fs)
: serial_(serial), fs_(fs), fq_(DEFAULT_FQ), gain_(DEFAULT_GAIN),
  dev_(nullptr), run_(false), counter_(0), state_(State::IDLE)  {}


AirspyDev::~AirspyDev(void) {
    assert(run_ == false);
}


int AirspyDev::start() {
    if (run_) return AIRSPYDEV_ALREADY_STARTED;

    state_ = State::STARTING;
    run_ = true;
    worker_thread_ = std::thread(worker_, std::ref(*this));

    return AIRSPYDEV_OK;
}


int AirspyDev::stop() {
    if (!run_) return AIRSPYDEV_ALREADY_STOPPED;

    run_ = false;
    worker_thread_.join();

    return AIRSPYDEV_OK;
}


int AirspyDev::setFq(uint32_t fq) {
    int ret;

    if (fq < MIN_FQ || fq > MAX_FQ) return AIRSPYDEV_INVALID_FQ;

    fq_ = fq;

    // Potential race here if the worker thread is restarting the device...
    if (dev_) {
        ret = airspy_set_freq((struct airspy_device*)dev_, fq_);
        if (ret != AIRSPY_SUCCESS) {
            return -1;
        }
    }

    return AIRSPYDEV_OK;
}


int AirspyDev::setTunerGain(float gain) {
    if (gain < MIN_GAIN || gain > MAX_GAIN) return AIRSPYDEV_INVALID_GAIN;

    gain_ = gain;

    return AIRSPYDEV_OK;
}


void AirspyDev::worker_(AirspyDev &self) {
    int ret;

    while (self.run_) {
        ret = self.open_();
        if (ret == AIRSPYDEV_OK) {
            std::cerr << "Device " << self.serial_ << " opended successfully\n";

            ret = airspy_start_rx((struct airspy_device*)self.dev_,
                                  reinterpret_cast<airspy_sample_block_cb_fn>(AirspyDev::data_cb_),
                                  &self);
            if (ret == AIRSPY_SUCCESS) {
                self.state_ = State::RUNNING;
                while (self.run_ && airspy_is_streaming((struct airspy_device*)self.dev_) == AIRSPY_TRUE) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                airspy_stop_rx((struct airspy_device*)self.dev_);

                if (self.run_) {
                    self.state_ = State::RESTARTING;
                    std::cerr << "Device " << self.serial_ << " disappeared. Trying to reopen...\n";
                }
            }

            airspy_close((struct airspy_device*)self.dev_);
            self.dev_ = nullptr;
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    self.state_ = State::IDLE;
}


int AirspyDev::open_(void) {
    int      ret = AIRSPYDEV_OK;
    uint64_t serial;

    if (serial_.length() != 0) {
        ret = sscanf(serial_.c_str(), "%" SCNx64, &serial);
        if (ret != 1) return AIRSPYDEV_INVALID_SERIAL;
    } else {
        serial = 0;
    }

    ret = airspy_open_sn((struct airspy_device**)&dev_, serial);
    if (ret == AIRSPY_SUCCESS) {
        ret = airspy_set_sample_type((struct airspy_device*)dev_, AIRSPY_SAMPLE_FLOAT32_IQ);
        if (ret == AIRSPY_SUCCESS) {
            ret = airspy_set_samplerate((struct airspy_device*)dev_, fs_);
            if (ret == AIRSPY_SUCCESS) {
                ret = airspy_set_freq((struct airspy_device*)dev_, fq_);
                if (ret == AIRSPY_SUCCESS) {
                    ret = AIRSPYDEV_OK;
                } else {
                    airspy_close((struct airspy_device*)dev_);
                    dev_ = nullptr;
                    ret = AIRSPYDEV_INVALID_FS;
                }
            } else {
                airspy_close((struct airspy_device*)dev_);
                dev_ = nullptr;
                ret = AIRSPYDEV_INVALID_FS;
            }
        } else {
            airspy_close((struct airspy_device*)dev_);
            dev_ = nullptr;
            ret = AIRSPYDEV_ERROR;
        }
    } else {
        ret = AIRSPYDEV_UNABLE_TO_OPEN_DEVICE;
    }

    return ret;
}


int AirspyDev::data_cb_(void *t) {
    airspy_transfer_t *transfer = reinterpret_cast<airspy_transfer_t*>(t);
    AirspyDev &self = *reinterpret_cast<AirspyDev*>(transfer->ctx);

    if (!self.run_) {
        return 0;
    }

    if (++self.counter_ == 40) {
        std::cerr << "data_cb_: device = " << self.serial_ << ", sample_count = " <<
                     transfer->sample_count << ", dropped_samples = " << transfer->dropped_samples << std::endl;
        self.counter_ = 0;
    }

    // Return 0 to continue or != 0 to stop
    return 0;
}


std::string AirspyDev::errStr(int ret) {
    switch (ret) {
        case AIRSPYDEV_OK:                    return "Ok";
        case AIRSPYDEV_ERROR:                 return "Error";
        case AIRSPYDEV_NO_DEVICE_FOUND:       return "No device found";
        case AIRSPYDEV_UNABLE_TO_OPEN_DEVICE: return "Unable to open device";
        case AIRSPYDEV_INVALID_FQ:            return "Invalid frequency";
        case AIRSPYDEV_INVALID_GAIN:          return "Invalid gain";
        case AIRSPYDEV_INVALID_SERIAL:        return "Invalid serial number format";
        case AIRSPYDEV_INVALID_FS:            return "Invalid sample rate";
        default:                              return "Unknown";
    }
}


std::vector<AirspyDev::Info> AirspyDev::list(void) {
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
            Info info;

            snprintf(serial_str, MAX_SERSTR_LEN, "%" PRIX64, serials[d]);

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
                    info.supported = true;
                    for (uint32_t i = 0; i < num_samplerates; ++i) {
                        info.sample_rates.push_back(sample_rates[i]);
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


bool AirspyDev::present(const std::string &serial) {
#define MAX_NUM_DEVICES   32
#define MAX_SERSTR_LEN    255
    int                   num_devices = 0;
    uint64_t              serials[MAX_NUM_DEVICES];
    char                  serial_str[MAX_SERSTR_LEN];

    num_devices = airspy_list_devices(serials, MAX_NUM_DEVICES);
    if (num_devices > 0 && num_devices <= MAX_NUM_DEVICES) {
        for (int d = 0; d < num_devices; ++d) {

            snprintf(serial_str, MAX_SERSTR_LEN, "%" PRIX64, serials[d]);

            if (serial == serial_str) return true;
        }
    }

    return false;
}
