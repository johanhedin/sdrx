//
// Base class for for RTL and Airspy tuner classes
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

#include "r820_dev.hpp"
#include "rtl_dev.hpp"
#include "airspy_dev.hpp"


R820Dev::R820Dev(const std::string &serial, SampleRate fs)
 : serial_(serial), fs_(fs), state_(State::IDLE), user_data_(nullptr), run_(false), type_(Type::UNKNOWN) {}


R820Dev::~R820Dev(void) {
    // Destroying the instance while is still running is considered a
    // programming error
    assert(run_ == false);
    assert(state_ == State::IDLE);
}


//
// Static functions below
//

R820Dev *R820Dev::create(Type type, const std::string &serial, SampleRate fs, int xtal_corr) {
    R820Dev *dev_ptr;

    switch (type) {
        case Type::RTL:
            dev_ptr = new RtlDev(serial, fs, xtal_corr);
            break;

        case Type::AIRSPY:
            dev_ptr = new AirspyDev(serial, fs);
            break;

        default:
            dev_ptr = nullptr;
            break;
    }

    if (dev_ptr) dev_ptr->type_ = type;

    return dev_ptr;
}


const std::string &R820Dev::retToStr(int ret) {
    static const std::string OK_STR("Ok");
    static const std::string ERROR_STR("Error");
    static const std::string DEVICE_NOT_FOUND_STR("Device not found");
    static const std::string UNABLE_TO_OPEN_DEVICE_STR("Unable to open device");
    static const std::string INVALID_SAMPLE_RATE_STR("Invalid sample rate");
    static const std::string INVALID_FQ_STR("Invalid frequency");
    static const std::string INVALID_GAIN_STR("Invalid gain");
    static const std::string INVALID_SERIAL_STR("Invalid serial");
    static const std::string ALREADY_STARTED_STR("Already started");
    static const std::string ALREADY_STOPPED_STR("Already stopped");
    static const std::string UNKNOWN_STR("Unknown");

    switch (ret) {
        case OK:                    return OK_STR;
        case ERROR:                 return ERROR_STR;
        case DEVICE_NOT_FOUND:      return DEVICE_NOT_FOUND_STR;
        case UNABLE_TO_OPEN_DEVICE: return UNABLE_TO_OPEN_DEVICE_STR;
        case INVALID_SAMPLE_RATE:   return INVALID_SAMPLE_RATE_STR;
        case INVALID_FQ:            return INVALID_FQ_STR;
        case INVALID_GAIN:          return INVALID_GAIN_STR;
        case INVALID_SERIAL:        return INVALID_SERIAL_STR;
        case ALREADY_STARTED:       return ALREADY_STARTED_STR;
        case ALREADY_STOPPED:       return ALREADY_STOPPED_STR;
        default:                    return UNKNOWN_STR;
    }
}


const std::string &R820Dev::typeToStr(Type type) {
    static const std::string UNKNOWN_STR("Unknown");
    static const std::string RTL_STR("RTL");
    static const std::string AIRSPY_STR("Airspy");

    switch (type) {
        case Type::RTL:    return RTL_STR;
        case Type::AIRSPY: return AIRSPY_STR;
        default:           return UNKNOWN_STR;
    }
}


/*
bool R820Dev::isPresent(const std::string &serial) {
    bool present = false;

    if (RtlDev::isPresent(serial)) {
        present = true;
    } else if (AirspyDev::isPresent(serial)) {
        present = true;
    }

    return present;
}
*/


R820Dev::Type R820Dev::getType(const std::string &serial) {
    Type type = Type::UNKNOWN;

    if (RtlDev::isPresent(serial)) {
        type = Type::RTL;
    } else if (AirspyDev::isPresent(serial)) {
        type = Type::AIRSPY;
    }

    return type;
}

std::vector<R820Dev::Info> R820Dev::list(void) {
    std::vector<R820Dev::Info> devices;
    std::vector<R820Dev::Info> devices_tmp;

    devices = RtlDev::list();
    devices_tmp = AirspyDev::list();

    devices.insert(devices.end(), devices_tmp.begin(), devices_tmp.end());

    return devices;
}

