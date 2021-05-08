
#ifndef RTLDEVCIE_HPP
#define RTLDEVCIE_HPP

#include <cstdint>
#include <string>
#include <thread>

struct rtlsdr_dev;


class RtlDev {
public:
    RtlDev(unsigned id = 0, int xtal_corr = 0);
    RtlDev(const std::string &serial, int xtal_corr = 0);
    ~RtlDev(void);

    int open(void);
    int start(void);

    int setFq(uint32_t fq = 100000000);
    int setSampleRate(uint32_t rate = 1200000);
    int setTunerGain(float gain = 30.0f);

    int stop(void);
    int close(void);

    static int list(void);

    RtlDev(const RtlDev&) = delete;
    RtlDev& operator=(const RtlDev&) = delete;

private:
    struct rtlsdr_dev *dev_;
    unsigned           id_;
    std::string        serial_;
    int                xtal_corr_;
    uint32_t           fq_;
    uint32_t           sample_rate_;
    float              gain_;
    bool               run_;
    std::thread        worker_thread_;
    static void        worker_(RtlDev &self);
    static void        data_cb_(unsigned char *data, uint32_t data_len, void *ctx);
};

#endif // RTLDEVCIE_HPP
