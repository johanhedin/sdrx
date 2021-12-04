//
// Interface class for controlling RTL and Airspy devices
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

#ifndef R820_DEV_HPP
#define R820_DEV_HPP

#include <vector>
#include <cstdint>
#include <string>
#include <chrono>

#include <sigc++/sigc++.h>

#include "iqsample.hpp"

// Supported sample rates in sdrx. Combination of what work for RTL dongles,
// Airspy devices and the internals of sdrx (M denotes downsampling factor to
// get to 16kHz).
enum class SampleRate {
    FS00960,      /// 0.96 MS/s RTL, M =  60
    FS01200,      //  1.2  MS/s RTL, M =  75
    FS01440,      //  1.44 MS/s RTL, M =  90
    FS01600,      //  1.6  MS/s RTL, M = 100
    FS01920,      //  1.92 MS/s RTL, M = 120
    FS02400,      //  2.4  MS/s RTL, M = 150
    FS02500,      //  2.5  MS/s Airspy R2
    FS02560,      //  2.56 MS/s RTL, M = 160
    FS03000,      //  3.0  MS/s Airspy Mini
    FS06000,      //  6.0  MS/s Airspy Mini (and R2 as alt. fs), M = 375
    FS10000,      // 10.0  MS/s Airspy R2 (and Mini as alt. fs), M = 625
    UNSPECIFIED
};


static inline const char *sample_rate_to_str(SampleRate rate) {
    switch (rate) {
        case SampleRate::FS00960: return "0.96";
        case SampleRate::FS01200: return "1.2";
        case SampleRate::FS01440: return "1.44";
        case SampleRate::FS01600: return "1.6";
        case SampleRate::FS01920: return "1.92";
        case SampleRate::FS02400: return "2.4";
        case SampleRate::FS02500: return "2.5";
        case SampleRate::FS02560: return "2.56";
        case SampleRate::FS03000: return "3";
        case SampleRate::FS06000: return "6";
        case SampleRate::FS10000: return "10";
        default:                  return "Unspecified";
    }
}


static inline SampleRate str_to_sample_rate(const std::string &str) {
    if      (str == "0.96") return SampleRate::FS00960;
    else if (str == "1.2")  return SampleRate::FS01200;
    else if (str == "1.44") return SampleRate::FS01440;
    else if (str == "1.6")  return SampleRate::FS01600;
    else if (str == "1.92") return SampleRate::FS01920;
    else if (str == "2.4")  return SampleRate::FS02400;
    else if (str == "2.5")  return SampleRate::FS02500;
    else if (str == "2.56") return SampleRate::FS02560;
    else if (str == "3")    return SampleRate::FS03000;
    else if (str == "6")    return SampleRate::FS06000;
    else if (str == "10")   return SampleRate::FS10000;
    else                    return SampleRate::UNSPECIFIED;
}


static inline uint32_t sample_rate_to_uint(SampleRate rate) {
    switch (rate) {
        case SampleRate::FS00960: return   960000;
        case SampleRate::FS01200: return  1200000;
        case SampleRate::FS01440: return  1440000;
        case SampleRate::FS01600: return  1600000;
        case SampleRate::FS01920: return  1920000;
        case SampleRate::FS02400: return  2400000;
        case SampleRate::FS02500: return  2500000;
        case SampleRate::FS02560: return  2560000;
        case SampleRate::FS03000: return  3000000;
        case SampleRate::FS06000: return  6000000;
        case SampleRate::FS10000: return 10000000;
        default:                  return        0;
    }
}


static inline SampleRate uint_to_sample_rate(uint32_t value) {
    if      (value ==   960000) return SampleRate::FS00960;
    else if (value ==  1200000) return SampleRate::FS01200;
    else if (value ==  1440000) return SampleRate::FS01440;
    else if (value ==  1600000) return SampleRate::FS01600;
    else if (value ==  1920000) return SampleRate::FS01920;
    else if (value ==  2400000) return SampleRate::FS02400;
    else if (value ==  2500000) return SampleRate::FS02500;
    else if (value ==  2560000) return SampleRate::FS02560;
    else if (value ==  3000000) return SampleRate::FS03000;
    else if (value ==  6000000) return SampleRate::FS06000;
    else if (value == 10000000) return SampleRate::FS10000;
    else                        return SampleRate::UNSPECIFIED;
}


// The three gain settings available in the R820T(2) tuner; LNA, Mixer and VGA.
// The index (0..15) in the array represent the register value.
// Values from http://steve-m.de/projects/rtl-sdr/gain_measurement/r820t
static const float lna_gain_steps[] = { 0.0f, 0.9f, 1.3f, 4.0f, 3.8f, 1.3f, 3.1f, 2.2f, 2.6f, 3.1f, 2.6f, 1.4f, 1.9f, 0.5f, 3.5f,  1.3f };
static const float mix_gain_steps[] = { 0.0f, 0.5f, 1.0f, 1.0f, 1.9f, 0.9f, 1.0f, 2.5f, 1.7f, 1.0f, 0.8f, 1.6f, 1.3f, 0.6f, 0.3f, -0.8f };
static const float vga_gain_steps[] = { 0.0f, 2.6f, 2.6f, 3.0f, 4.2f, 3.5f, 2.4f, 1.3f, 1.4f, 3.2f, 3.6f, 3.4f, 3.5f, 3.7f, 3.5f,  3.6f };


class R820Dev {
public:
    // Device types that this interface class support
    enum class Type { UNKNOWN, RTL, AIRSPY };

    // Struct for information about a device on the system
    struct Info {
        Type                    type;
        unsigned                index;
        std::string             serial;
        bool                    available;
        bool                    supported;
        std::string             description;
        std::vector<SampleRate> sample_rates;
        SampleRate              default_sample_rate;
    };

    // Return values from the class member functions
    enum ReturnValue {
        OK                    =   0,
        ERROR                 =  -1,
        DEVICE_NOT_FOUND      =  -2,
        UNABLE_TO_OPEN_DEVICE =  -3,
        INVALID_SAMPLE_RATE   =  -4,
        INVALID_FQ            =  -5,
        INVALID_GAIN          =  -6,
        INVALID_SERIAL        =  -7,
        ALREADY_STARTED       =  -8,
        ALREADY_STOPPED       =  -9
    };

    // States for the device manager
    enum class State { IDLE, STARTING, RUNNING, RESTARTING, STOPPING };

    // States for the streaming manager
    enum class StreamState { IDLE, STREAMING };

    // Struct holding information related to one block of IQ output from a device.
    class BlockInfo {
    public:
        using TimeStamp = std::chrono::time_point<std::chrono::system_clock>;

        // Streaming state
        StreamState stream_state;

        // Sample rate used
        SampleRate  rate;

        // Average signal power in the block expressed as dBFS relative to a
        // full scale sine wave
        float       pwr;

        // Timestamp (set by the host) for the last sample in the block
        TimeStamp   ts;
    };

    // Factory function for creating a new instance
    static R820Dev *create(Type type, const std::string &serial, SampleRate rate, int xtal_corr = 0);

    // Instances of this class is not intended to be copied in any way
    R820Dev(const R820Dev&) = delete;
    R820Dev& operator=(const R820Dev&) = delete;

    // Destructor may be implemented in an inherited class as well
    virtual ~R820Dev(void);

    // Get device type that this instance is controlling
    Type getType(void) { return type_; }

    // Associate opaque arbitrary user data with the instance.
    void setUserData(void *user_data = nullptr) { user_data_ = user_data; }

    // Start up the device manager asynchronous. This function will return
    // immediately and the internal thread will start looking for the
    // device requested in the constructor and start it. The getState()
    // member function can be used to monitor the progress.
    virtual int start(void) = 0;

    virtual int setFq(uint32_t fq = 100000000) = 0;
    virtual int setGain(float gain = 30.0f) = 0;
    virtual int setLnaGain(unsigned idx) = 0;
    virtual int setMixGain(unsigned idx) = 0;
    virtual int setVgaGain(unsigned idx) = 0;

    // Stop the device manager. This function will block until the worker thread
    // is stopped and the device is fully closed
    virtual int stop(void) = 0;

    // Get the current state of the device manager
    State getState(void) { return state_; }

    // Data signal. Emitted when a block of 32ms of data is available
    // irrespectively of the sample rate. Data len will ofcourse vary. 32ms
    // bocks equals a callback frequency of 31.25Hz. The signal will be
    // emitted in the context of an internal thread. Use appropriate mutex
    // in your slot if you need to.
    sigc::signal<void(const iqsample_t*, unsigned, void*, const BlockInfo&)> data;

    // Convert return value to string
    static const std::string &retToStr(int ret);

    // Convert device type to string
    static const std::string &typeToStr(Type type);

    // Get type of device given a serial. If the requested device serial is
    // not available on the bus, UNKNOWN is returned
    static Type getType(const std::string &serial);

    // Check if the given device supports the given rate
    static bool rateSupported(const std::string &serial, SampleRate rate);

    // Get a list of available devices
    static std::vector<R820Dev::Info> list(void);

protected:
    // Prevent instantiation of the base class
    R820Dev(const std::string&, SampleRate);

    std::string    serial_;
    SampleRate     fs_;
    State          state_;
    void          *user_data_;
    bool           run_;
    BlockInfo      block_info_;

private:
    Type           type_;
};

#endif // R820_DEV_HPP
