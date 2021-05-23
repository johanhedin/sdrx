//
// Device class for a RTL dongle
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

#ifndef RTL_DEV_HPP
#define RTL_DEV_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <vector>


enum rtl_dev_return_values {
    RTLDEV_OK                    =   0,
    RTLDEV_ERROR                 =  -1,
    RTLDEV_NO_DEVICE_FOUND       =  -2,
    RTLDEV_UNABLE_TO_OPEN_DEVICE =  -3,
    RTLDEV_INVALID_FQ            =  -4,
    RTLDEV_INVALID_GAIN          =  -5,
    RTLDEV_ALREADY_STARTED       =  -6,
    RTLDEV_ALREADY_STOPPED       =  -7
};


class RtlDev {
public:
    enum class State { IDLE, STARTING, RUNNING, RESTARTING, STOPPING };

    struct Info {
        unsigned              index;
        std::string           serial;
        bool                  available;
        bool                  supported;
        std::string           description;
        std::vector<uint32_t> sample_rates;
    };

    RtlDev(const std::string &serial, uint32_t fs = 1200000, int xtal_corr = 0);
    ~RtlDev(void);

    // Start up the instance asynchronous. This function will return
    // immediately and the internal thread will start looking for the requested
    // device
    int start(void);

    int setFq(uint32_t fq = 100000000);
    int setTunerGain(float gain = 30.0f);

    State getState(void) { return state_; }

    // Stop the instance. This function will block until the worker thread
    // is stopped and the RTL device is fully closed
    int stop(void);

    // Get a list of available devices by serial number
    static std::vector<RtlDev::Info> list(void);

    // Check if a given device is present on the USB bus
    static bool present(const std::string &serial);

    // Convert error code to string
    static std::string errStr(int ret);

    RtlDev(const RtlDev&) = delete;
    RtlDev& operator=(const RtlDev&) = delete;

private:
    std::string    serial_;
    uint32_t       fs_;
    int            xtal_corr_;
    uint32_t       fq_;
    float          gain_;
    void          *dev_;
    bool           run_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(RtlDev &self);
    static void    data_cb_(unsigned char *data, uint32_t data_len, void *ctx);
    unsigned       counter_;
    State          state_;
};

#endif // RTL_DEV_HPP
