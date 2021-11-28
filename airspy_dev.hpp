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

#include "r820_dev.hpp"


class AirspyDev : public R820Dev {
public:
    AirspyDev(const std::string &serial, SampleRate fs);

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

    // Stop the instance. This function will block until the worker thread
    // is stopped and the Airspy device is fully closed
    int stop(void);

    // Get a list of available devices
    static std::vector<R820Dev::Info> list(void);

    // Check if a given device is present on the USB bus
    static bool isPresent(const std::string &serial);

private:
    uint32_t       fq_;
    float          gain_;
    unsigned       lna_gain_idx_;
    unsigned       mix_gain_idx_;
    unsigned       vga_gain_idx_;
    void          *dev_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(AirspyDev &self);
    static int     data_cb_(void *transfer);
    iqsample_t     iq_buffer_[320000*2]; // Largest buffer for 10MS/s. Times twice
    unsigned       part_pos_;
    unsigned       block_size_;
    unsigned       iq_pos_;
};

#endif // AIRSPY_DEV_HPP
