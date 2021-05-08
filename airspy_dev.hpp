
#ifndef AIRSPYDEV_HPP
#define AIRSPYDEV_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <thread>


// Forward declaration
struct airspy_device;

class AirspyDev {
public:
    struct Info {
        std::string           serial;
        std::string           version;
        std::vector<unsigned> sample_rates;
    };

    AirspyDev(const std::string &serial);
    ~AirspyDev(void);

    const std::string &getSerial(void) const { return serial_; }

    int open(void);
    int start(void);

    int setFq(uint32_t fq) { return 0; }
    int setFs(uint32_t fs) { return 0; }
    int setTunerGain(float tuner_gain) { return 0; }

    int stop(void);
    int close(void);

    // Get a list of available devices by serial number
    static std::vector<std::string> list(void);

    AirspyDev(const AirspyDev&) = delete;
    AirspyDev& operator=(const AirspyDev&) = delete;

private:
    airspy_device *dev_;
    std::string    serial_;
    bool           run_;
    std::thread    worker_thread_;
    static void    worker_(AirspyDev &self);
};

#endif // AIRSPYDEV_HPP
