
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
    RTLDEV_INVALID_GAIN          =  -5
};


class RtlDev {
public:
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

    int open(void);
    int start(void);

    int setFq(uint32_t fq = 100000000);
    int setTunerGain(float gain = 30.0f);

    int stop(void);
    int close(void);

    // Get a list of available devices by serial number
    static std::vector<RtlDev::Info> list(void);
    static bool present(const std::string &serial);
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
};

#endif // RTL_DEV_HPP
