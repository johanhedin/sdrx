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

#include <sigc++/sigc++.h>

#include "common_dev.hpp"
#include "iqsample.hpp"


enum rtl_dev_return_values {
    RTLDEV_OK                    =   0,
    RTLDEV_ERROR                 =  -1,
    RTLDEV_DEVICE_NOT_FOUND      =  -2,
    RTLDEV_UNABLE_TO_OPEN_DEVICE =  -3,
    RTLDEV_INVALID_SAMPLE_RATE   =  -4,
    RTLDEV_INVALID_FQ            =  -5,
    RTLDEV_INVALID_GAIN          =  -6,
    RTLDEV_INVALID_SERIAL        =  -7,
    RTLDEV_ALREADY_STARTED       =  -8,
    RTLDEV_ALREADY_STOPPED       =  -9
};


class RtlDev {
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

    RtlDev(const std::string &serial, SampleRate fs, int xtal_corr = 0);
    ~RtlDev(void);

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
    // is stopped and the RTL device is fully closed
    int stop(void);

    // Get a list of available devices
    static std::vector<RtlDev::Info> list(void);

    // Check if a given device is present on the USB bus
    static bool isPresent(const std::string &serial);

    // Convert error code to string
    static std::string errStr(int ret);

    RtlDev(const RtlDev&) = delete;
    RtlDev& operator=(const RtlDev&) = delete;

private:
    std::string    serial_;
    SampleRate     fs_;
    int            xtal_corr_;
    uint32_t       fq_;
    float          gain_;
    unsigned       lna_gain_idx_;
    unsigned       mix_gain_idx_;
    unsigned       vga_gain_idx_;
    void          *dev_;
    bool           run_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(RtlDev &self);
    static void    data_cb_(unsigned char *data, uint32_t data_len, void *ctx);
    State          state_;
    iqsample_t     iq_buffer_[81920];   // Largest buffer for 2.56MS/s
};

#endif // RTL_DEV_HPP
