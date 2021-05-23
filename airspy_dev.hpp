
#ifndef AIRSPY_DEV_HPP
#define AIRSPY_DEV_HPP

#include <cstdint>
#include <string>
#include <thread>
#include <vector>


enum airspy_dev_return_values {
    AIRSPYDEV_OK                    =   0,
    AIRSPYDEV_ERROR                 =  -1,
    AIRSPYDEV_NO_DEVICE_FOUND       =  -2,
    AIRSPYDEV_UNABLE_TO_OPEN_DEVICE =  -3,
    AIRSPYDEV_INVALID_FQ            =  -4,
    AIRSPYDEV_INVALID_GAIN          =  -5,
    AIRSPYDEV_INVALID_SERIAL        =  -6,
    AIRSPYDEV_INVALID_FS            =  -7
};


class AirspyDev {
public:
    struct Info {
        unsigned              index;
        std::string           serial;
        bool                  available;
        bool                  supported;
        std::string           description;
        std::vector<uint32_t> sample_rates;
    };

    AirspyDev(const std::string &serial, uint32_t fs);
    ~AirspyDev(void);

    int open(void);
    int start(void);

    int setFq(uint32_t fq = 100000000);
    int setTunerGain(float gain = 30.0f);

    int stop(void);
    int close(void);

    // Get a list of available devices by serial number
    static std::vector<AirspyDev::Info> list(void);
    static bool present(const std::string &serial);
    static std::string errStr(int ret);

    AirspyDev(const AirspyDev&) = delete;
    AirspyDev& operator=(const AirspyDev&) = delete;

private:
    std::string    serial_;
    uint32_t       fs_;
    uint32_t       fq_;
    float          gain_;
    void          *dev_;
    bool           run_;
    std::thread    worker_thread_;
    int            open_(void);
    static void    worker_(AirspyDev &self);
    static int     data_cb_(void *transfer);
    unsigned       counter_;
};

#endif // AIRSPY_DEV_HPP
