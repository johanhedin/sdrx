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

#ifndef AIRSPY_DEV_HPP
#define AIRSPY_DEV_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include <sigc++/sigc++.h>

#include "common_dev.hpp"
#include "iqsample.hpp"


enum airspy_dev_return_values {
    AIRSPYDEV_OK                    =   0,
    AIRSPYDEV_ERROR                 =  -1,
    AIRSPYDEV_NO_DEVICE_FOUND       =  -2,
    AIRSPYDEV_UNABLE_TO_OPEN_DEVICE =  -3,
    AIRSPYDEV_INVALID_SAMPLE_RATE   =  -4,
    AIRSPYDEV_INVALID_FQ            =  -5,
    AIRSPYDEV_INVALID_GAIN          =  -6,
    AIRSPYDEV_INVALID_SERIAL        =  -7,
    AIRSPYDEV_ALREADY_STARTED       =  -8,
    AIRSPYDEV_ALREADY_STOPPED       =  -9
};


class AirspyDev {
public:
    enum class State { IDLE, STARTING, RUNNING, RESTARTING, STOPPING };

    struct Info {
        unsigned                index;
        std::string             serial;
        bool                    available;
        bool                    supported;
        std::string             description;
        std::vector<SampleRate> sample_rates;
    };

    AirspyDev(const std::string &serial, SampleRate fs);
    ~AirspyDev(void);

    // Start up the instance asynchronous. This function will return
    // immediately and the internal thread will start looking for the
    // device requested in the constructor and start it. The getState()
    // member function can be used to monitor the progress.
    int start(void);

    int setFq(uint32_t fq = 100000000);
    int setGain(float gain = 30.0f);

    int setLnaGain(unsigned idx);
    int setMixGain(unsigned idx);
    int setVgaGain(unsigned idx);

    // Data signal. One block represents 32ms of data irrespectively of the
    // sampling frequency. Data len will ofcourse vary. 32ms bocks equals
    // a callback frequency of 31.25Hz
    sigc::signal<void(const iqsample_t*, unsigned, SampleRate)> data;

    State getState(void) { return state_; }

    // Stop the instance. This function will block until the worker thread
    // is stopped and the Airspy device is fully closed
    int stop(void);

    // Get a list of available devices
    static std::vector<AirspyDev::Info> list(void);

    // Check if a given device is present on the USB bus
    static bool isPresent(const std::string &serial);

    // Convert error code to string
    static std::string errStr(int ret);

    AirspyDev(const AirspyDev&) = delete;
    AirspyDev& operator=(const AirspyDev&) = delete;

private:
    std::string    serial_;
    SampleRate     fs_;
    uint32_t       fq_;
    float          gain_;
    unsigned       lna_gain_idx_;
    unsigned       mix_gain_idx_;
    unsigned       vga_gain_idx_;
    void          *dev_;
    bool           run_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(AirspyDev &self);
    static int     data_cb_(void *transfer);
    State          state_;
    iqsample_t     iq_buffer_[320000*2]; // Largest buffer for 10MS/s. Times twice
    unsigned       part_pos_;
    unsigned       block_size_;
    unsigned       iq_pos_;
};

#endif // AIRSPY_DEV_HPP
