
#ifndef RTLDEVCIE_HPP
#define RTLDEVCIE_HPP

#include <cstdint>
#include <string>
#include <thread>


class RtlDevice {
public:
    RtlDevice(unsigned id = 0);
    RtlDevice(const std::string &serial);
    ~RtlDevice(void);

    int open(void);
    int start(void);

    int setFq(uint32_t fq) { return 0; }
    int setFs(uint32_t fs) { return 0; }
    int setXtalCorr(int xtal_corr) { return 0; }
    int setTunerGain(float tuner_gain) { return 0; }

    int stop(void);
    int close(void);

    static int list(void);

    RtlDevice(const RtlDevice&) = delete;
    RtlDevice& operator=(const RtlDevice&) = delete;

private:
    class rtlsdr_dev_t;
    rtlsdr_dev_t *dev_;
    unsigned      id_;
    std::string   serial_;
    bool          run_;
    std::thread   worker_thread_;

    static void   data_cb_(unsigned char *data, uint32_t data_len, void *ctx);
    static void   worker_(RtlDevice &self);

    int           dummy_;
};

#endif // RTLDEVCIE_HPP
