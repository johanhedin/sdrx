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

#ifndef RTL_DEV_HPP
#define RTL_DEV_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <vector>

#include "r820_dev.hpp"


class RtlDev : public R820Dev {
public:
    RtlDev(const std::string &serial, SampleRate fs, int xtal_corr = 0);

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
    // is stopped and the RTL device is fully closed
    int stop(void);

    // Get a list of available devices
    static std::vector<R820Dev::Info> list(void);

    // Check if a given device is present on the USB bus
    static bool isPresent(const std::string &serial);

private:
    int            xtal_corr_;
    uint32_t       fq_;
    float          gain_;
    unsigned       lna_gain_idx_;
    unsigned       mix_gain_idx_;
    unsigned       vga_gain_idx_;
    void          *dev_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(RtlDev &self);
    static void    data_cb_(unsigned char *data, uint32_t data_len, void *ctx);
    iqsample_t     iq_buffer_[81920];   // Largest buffer for 2.56MS/s
};

#endif // RTL_DEV_HPP
