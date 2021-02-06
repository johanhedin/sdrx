//
// Software Defined Receiver.
//
// @author Johan Hedin
// @date   2021-01-13
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

// Standard C includes
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <poll.h>
#include <sys/time.h>
#include <inttypes.h>

// Standard C++ includes
#include <thread>
#include <mutex>
#include <atomic>
#include <new>
#include <cstring>
#include <type_traits>
#include <iostream>
#include <algorithm>
#include <utility>
#include <cmath>
#include <cstdint>
#include <clocale>
#include <cctype>
#include <map>

// Libs that we use
#include <popt.h>
#include <rtl-sdr.h>
#include <alsa/asoundlib.h>
#include <fftw3.h>

// Local includes
#include "coeffs.hpp"
#include "msd.hpp"
#include "rb.hpp"
#include "fir.hpp"
#include "agc.hpp"

// Usefull notes:
// libusb and threads: http://libusb.sourceforge.net/api-1.0/libusb_mtasync.html
// AM demodulation: https://www.diva-portal.org/smash/get/diva2:831827/FULLTEXT01.pdf
// AGC: https://www.eit.lth.se/fileadmin/eit/courses/etin80/2017/reports/adaptive-gain-control.pdf
// Power spectra, windowing etc: https://pysdr.org/content/sampling.html
// ALSA: https://alsa.opensrc.org/Asynchronous_Playback_(Howto)
//

#define AERONAUTICAL_CHANNEL true
#define NORMAL_FQ            false

// Interval in seconds between statistics output
#define STAT_INTERVAL  5


// Size of the sample callback buffer. This will become len in the data
// callback.
//
// Must be a multiple of 512. Shoulb be a multiple of 16384 (URB size).
//
// The larger this is set, the more seldom the iq_cb is called.
//#define IQ_BUF_SIZE 6*16384

// Sampling, decimation. latency and IQ buffer size setup
//
// A sample frequency that is evenly divided with the TCXO clock is advicable.
// The dongle clock runs at 28.8MHz. This gives "nice" IQ sampling frequenies
// of 2 400 000 (1/12th), 1 800 000 (1/16th) 1 200 000 (1/24th),
// 960 000 (1/30th) and so on.
//
// To get down to the signal of interest we like to get to a 16000 IQ sampling
// frequency (16kHz RF bandwidth) by using decimation.
//
// For FFT calculations, batches of power of 2 number of samples are most
// efficent.
//
// The IQ data must be fetched in blocks 512 bytes, or 256 IQ samples. So if
// we set the dongle USB buffer to the decimation factor times 512 we will
// get 256 decimated IQ samples at 16kHz (16kHz RF BW) per "callback period".
//
// The callback period is 1/Fs * (buffer_size/2) seconds.
//
// Due to latency we like to have the callback period around 20ms.
#define SAMPLING_FREQUENCY 1200000
#define DECIMATION_FACTOR  75
#define IQ_BUF_SIZE        (512 * DECIMATION_FACTOR)
#define DECIMATED_SIZE     256


// Name of the output sound device
#define ALSA_DEVICE "default"

static int quit = 0;
static struct timeval last_stat_calc;
static uint32_t bytes;
static uint32_t samples;
static uint32_t frames;
static std::atomic_flag cout_lock = ATOMIC_FLAG_INIT;

// Datatype to hold the sdrx settins
class Settings {
public:
    Settings(void) : rtl_device(0), fq_corr(0), rf_gain(30.0f), lf_gain(3.0f), sql_level(15.0f), audio_device("default"), fq(0) {}
    int         rtl_device;   // RTL-SDR device to use (in id form)
    int         fq_corr;      // Frequency correction in ppm
    float       rf_gain;      // RF gain in dB
    float       lf_gain;      // LF (audio gain) in dB
    float       sql_level;    // Squelch level in dB (over noise level)
    std::string audio_device; // ALSA device to use for playback
    uint32_t    fq;           // Frequency to tune to
    std::string channel;      // String representation of the channel, e.g. "118.105"
};


#define LEVEL_SIZE 10
struct InputState {
    rtlsdr_dev_t   *rtl_device;           // RTL device
    MSD            *ds;                   // Downsampler
    RB<iqsample_t> *rb_ptr;               // Input -> Output buffer
    Settings        settings;             // System wide settings
    float           i_levels[LEVEL_SIZE];
    float           q_levels[LEVEL_SIZE];
    unsigned        num_levels;
};


#define FFT_SIZE 512
struct OutputState {
    snd_pcm_t         *pcm_handle;               // ALSA PCM device
    RB<iqsample_t>    *rb_ptr;                   // Input -> Output buffer
    struct timeval     last_stat_time;
    int16_t            silence[2048];
    float              audio_buffer_float[2048];
    int16_t            audio_buffer_s16[2048];
    FIR               *audio_filter;
    float              gain;
    iqsample_t         fft_in[FFT_SIZE];
    iqsample_t         fft_out[FFT_SIZE];
    float              window[FFT_SIZE+1];
    fftwf_plan         fft_plan;
    unsigned           sql_wait;
    unsigned           fft_samples;
    bool               sql_open;
    std::vector<float> hi_energy;
    std::vector<float> lo_energy;
    unsigned           energy_idx;
    AGC                agc;
    Settings           settings;
};


static void signal_handler(int signo) {
    std::cout << "Signal received. Stopping..." << std::endl;
    quit = 1;
}


static const char* rtlsdr_tuner_to_str(enum rtlsdr_tuner tuner_id) {
    switch (tuner_id) {
        case RTLSDR_TUNER_E4000:  return "E4000";
        case RTLSDR_TUNER_FC0012: return "FC0012";
        case RTLSDR_TUNER_FC0013: return "FC0013";
        case RTLSDR_TUNER_FC2580: return "FC2580";
        case RTLSDR_TUNER_R820T:  return "R820T(2)";
        case RTLSDR_TUNER_R828D:  return "R828D";
        default:                  return "Unknown";
    }
}


static snd_pcm_t *open_alsa_dev(const std::string &device_name) {
    int                  ret;
    snd_pcm_t           *handle = nullptr;
    snd_pcm_hw_params_t *pcm_hw_params = nullptr;
    snd_pcm_sw_params_t *pcm_sw_params = nullptr;
    snd_pcm_uframes_t    pcm_period;
    snd_pcm_uframes_t    pcm_buffer_size;
    snd_pcm_uframes_t    pcm_start_threshold;
    snd_pcm_uframes_t    pcm_note_threshold;
    snd_pcm_format_t     pcm_sample_format;
    snd_pcm_access_t     pcm_access;
    unsigned int         pcm_sample_rate;
    unsigned int         num_channels;

    // How we like our ALSA device to operate
    pcm_access = SND_PCM_ACCESS_RW_INTERLEAVED;  // Channel samples are interleaved
    pcm_sample_rate     = 16000;                 // 16kHz
    pcm_sample_format   = SND_PCM_FORMAT_S16;    // Signed 16 bit samples
    num_channels        = 1;                     // Mono
    pcm_period          = 512;                   // Play chunks of this many frames (512/16000 -> 32ms)
    pcm_buffer_size     = pcm_period * 8;        // Playback buffer is 4 periods, i.e. 128ms
    pcm_note_threshold  = pcm_period;            // Notify us when we can write at least this number of frames
    pcm_start_threshold = pcm_period*4;          // Start playing when this many frames have been written to the device buffer

    // Summary:
    // poll will return when we can write at least pcm_period samples. Actual
    // playback will not start until pcm_start_threshold samples has been
    // written to the device buffer

    std::cout << "Opening ALSA device '" << device_name << "' with:" << std::endl;
    std::cout << "    Sample rate: " << pcm_sample_rate << " samples/s" << std::endl;
    std::cout << "    Sample format: 16 bit signed integer" << std::endl;
    std::cout << "    Number of channels: " << num_channels << std::endl;
    std::cout << "    PCM Period: " << pcm_period << " samples (" << pcm_period / (pcm_sample_rate/1000) << "ms)" << std::endl;
    std::cout << "    Buffer size: " << pcm_buffer_size << " samples (" << pcm_buffer_size / (pcm_sample_rate/1000) << "ms)" << std::endl;
    std::cout << "    Wakeup low limit: " << pcm_note_threshold << " samples (" << pcm_note_threshold / (pcm_sample_rate/1000) << "ms)" << std::endl;
    std::cout << "    Start threshold: " << pcm_start_threshold << " samples (" << pcm_start_threshold / (pcm_sample_rate/1000) << "ms)" << std::endl;

    // Open device 'device_name' for playback
    ret = snd_pcm_open(&handle, device_name.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK);
    if (ret < 0) {
        std::cerr << "ALSA error from snd_pcm_open for device '" << device_name
                  << "': " << snd_strerror(ret) << std::endl;
        handle = nullptr;
        return handle;
    }

    // Allocate space for the hardare params config struct
    snd_pcm_hw_params_malloc(&pcm_hw_params);

    // Read the current hardware config from the device
    snd_pcm_hw_params_any(handle, pcm_hw_params);

    // Set hardware configurations
    snd_pcm_hw_params_set_access(handle, pcm_hw_params, pcm_access);
    snd_pcm_hw_params_set_format(handle, pcm_hw_params, pcm_sample_format);
    snd_pcm_hw_params_set_channels(handle, pcm_hw_params, num_channels);
    snd_pcm_hw_params_set_period_size(handle, pcm_hw_params, pcm_period, 0);
    snd_pcm_hw_params_set_buffer_size(handle, pcm_hw_params, pcm_buffer_size);

    // Probe sample rate to detect if the sound card is incapable of the
    // requested rate
    unsigned int tmp_sample_rate = pcm_sample_rate;
    ret = snd_pcm_hw_params_set_rate_near(handle, pcm_hw_params, &tmp_sample_rate, NULL);
    if (ret < 0) {
        std::cout << "ALSA error: Unable to set sample rate to " << pcm_sample_rate
                  << ". Got " << tmp_sample_rate << std::endl;
    }

    // Write hadware configuration to the device
    ret = snd_pcm_hw_params(handle, pcm_hw_params);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to set hw params: " << snd_strerror(ret) << std::endl;
    }

    // Free the hardware parameters struct
    snd_pcm_hw_params_free(pcm_hw_params);

    // Allocate space for the software params config struct
    snd_pcm_sw_params_malloc(&pcm_sw_params);

    // Read the current software config from the device
    snd_pcm_sw_params_current(handle, pcm_sw_params);

    ret = snd_pcm_sw_params_set_avail_min(handle, pcm_sw_params, pcm_note_threshold);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to set min available: " << snd_strerror(ret) << std::endl;
    }

    ret = snd_pcm_sw_params_set_start_threshold(handle, pcm_sw_params, pcm_start_threshold);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to set start threshold: " << snd_strerror(ret) << std::endl;
    }

    // Write software configuration to the device
    ret = snd_pcm_sw_params(handle, pcm_sw_params);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to set sw params: " << snd_strerror(ret) << std::endl;
    }

    // Free the software params config struct
    snd_pcm_sw_params_free(pcm_sw_params);

    return handle;
}


static void close_alsa_dev(snd_pcm_t *dev) {
    snd_pcm_close(dev);

    // ALSA will cache config in a global wide variable indicating memory
    // leaks if not freed
    snd_config_update_free_global();
}


static void alsa_write_cb(OutputState &ctx) {
    struct timeval    current_time;
    int16_t           sample_s16;
    size_t            samples_avail;
    const iqsample_t *iq_buffer;
    int               ret;

    gettimeofday(&current_time, NULL);
    ret = snd_pcm_avail_update(ctx.pcm_handle);
    if (ret < 0) {
        fprintf(stderr, "ALSA Error pcm_avail: %s\n", snd_strerror(ret));
        //snd_pcm_prepare(ctx.pcm_handle);
        //continue;
    } else {
        if (current_time.tv_sec > ctx.last_stat_time.tv_sec + 1) {
            //fprintf(stdout, "ALSA available space: %d\n", ret);
            ctx.last_stat_time = current_time;
        }
    }

    // Ok to write more data
    if (ctx.rb_ptr->acquireRead(&iq_buffer, &samples_avail)) {
        size_t samples_to_write = std::min(samples_avail, (size_t)256);

        if (samples_to_write != 256) {
            std::cout << "RB Error: did not get 256 samples. Got " << samples_to_write << std::endl;
        }

        for (size_t j = 0; j < samples_to_write; j++) {
            ctx.fft_in[ctx.fft_samples] = iq_buffer[j] * ctx.window[ctx.fft_samples];  // Fill FFT in buffer
            ctx.fft_samples++;
            ctx.audio_buffer_float[j] = std::abs(ctx.agc.adjust(iq_buffer[j])); // Demodulate AGC adjusted AM signal
        }
        ctx.rb_ptr->commitRead(samples_to_write);

        // Calculate fft. Compensate for scaling? (divide by FFT_SIZE?)
        //
        // Contents of fft_out:
        //
        // Index                         Contents
        // --------------------------------------------------------------------
        // 0                           : DC part
        // 1           -> FFT_SIZE/2-1 : Positive frequencies in increasing order
        // FFT_SIZE/2                  : Nyquist component. Common to both the negative and positive parts
        // FFT_SIZE/2+1 -> FFT_SIZE-1  : Negative frequencies in increasing order
        //
        // If the IQ signal is at 110.000MHz and Fs == 2MS/s we can represent
        // a band between 109.000 and 111.000 MHz. The sub-band 110 -> 111 is
        // in samples 1:FFT_SIZE/2-1 and the sub-band 109 -> 110 is in samples
        // FFT_SIZE/2+1:FFT_SIZE-1
        //
        // Since our sampling frequency is 16kHz and we are processing IQ,
        // the "band" we observe is fc +/- 8Khz and every bin represent
        // 16000/FFT_SIZE (31.25Hz for a fft size of 512). Index 0 is thus
        // fc, index 1 fc + 31.25 and index FFT_SIZE-1 is fc - 31.25.
        //
        // http://www.fftw.org/fftw3.pdf chapter 4.8
        // https://www.gaussianwaves.com/2015/11/interpreting-fft-results-complex-dft-frequency-bins-and-fftshift
        //
        if (ctx.fft_samples == FFT_SIZE) {
            fftwf_execute(ctx.fft_plan);

            // Energy/power calculations for squelch and spectral imbalance
            float sig_level = 0.0f;
            for (unsigned i = 3; i < 91; i++) {
                // About 2.8kHz +/- Fc
                sig_level += std::norm(ctx.fft_out[i]);
                sig_level += std::norm(ctx.fft_out[FFT_SIZE - i]);
            }
            // Including DC seem to increase base level with ~5dB
            //sig_level += std::norm(ctx.fft_out[0]);
            sig_level /= 176;

            float ref_level_hi = 0.0f;
            float ref_level_lo = 0.0f;
            for (unsigned i = 112; i < 157; i++) {
                // About 3.5kHz to 4.9kHz
                ref_level_hi += std::norm(ctx.fft_out[i]) * passband_shape[i];
                ref_level_lo += std::norm(ctx.fft_out[FFT_SIZE - i]) * passband_shape[FFT_SIZE - i];
                //ref_level_hi += std::norm(ctx.fft_out[i]);
                //ref_level_lo += std::norm(ctx.fft_out[FFT_SIZE - i]);
            }
            ref_level_hi /= 45;
            ref_level_lo /= 45;
            float noise_level = (ref_level_hi + ref_level_lo) / 2;

            // Determine spectral imbalance (indicating frequency offset between signal and receiver fqs)
            float lo_energy = 0.0f;
            float hi_energy = 0.0f;
            for (unsigned i = 1; i < FFT_SIZE/2; i++) {
                hi_energy += std::norm(ctx.fft_out[i]);
                lo_energy += std::norm(ctx.fft_out[i+FFT_SIZE/2]);
            }

            ctx.lo_energy[ctx.energy_idx] = lo_energy / 255;
            ctx.hi_energy[ctx.energy_idx] = hi_energy / 255;
            if (++ctx.energy_idx == 10) ctx.energy_idx = 0;

            // Calculate SNR
            float snr = 10 * std::log10(sig_level / noise_level);

            // Convert levels to dB
            sig_level    = 10 * std::log10(sig_level);
            ref_level_hi = 10 * std::log10(ref_level_hi);
            ref_level_lo = 10 * std::log10(ref_level_lo);

            if (snr > ctx.settings.sql_level) {
                ctx.sql_open = true;
            } else {
                ctx.sql_open = false;
            }

            if (++ctx.sql_wait >= 10) {
                lo_energy = 0.0f;
                hi_energy = 0.0f;
                for (unsigned i = 0; i < 10; ++i) {
                    lo_energy += ctx.lo_energy[i];
                    hi_energy += ctx.hi_energy[i];
                }

                lo_energy /= 10;
                hi_energy /= 10;

                float imbalance = hi_energy - lo_energy;

                fprintf(stdout, "%s %s. [lo|mid|hi|SNR|imbalance]: %6.2f|%6.2f|%6.2f|%6.2f|%6.2f\n",
                        ctx.settings.channel.c_str(), ctx.sql_open ? "  open":"closed",
                        ref_level_lo, sig_level, ref_level_hi, snr, imbalance);

                ctx.sql_wait = 0;
            }

            ctx.fft_samples = 0;
        }

        // Final audio filter
        ctx.audio_filter->filter(ctx.audio_buffer_float, samples_to_write, ctx.audio_buffer_float);

        for (size_t j = 0; j < samples_to_write; j++) {
            // Calculate S16 sample
            if (ctx.audio_buffer_float[j] > 1.0f) {
                sample_s16 = 32767;
            } else if (ctx.audio_buffer_float[j] < -1.0f) {
                sample_s16 = -32767;
            } else {
                sample_s16 = (int16_t)(ctx.audio_buffer_float[j] * 32767.0f);
            }
            ctx.audio_buffer_s16[j] = sample_s16;
        }

        if (ctx.sql_open) {
            ret = snd_pcm_writei(ctx.pcm_handle, ctx.audio_buffer_s16, samples_to_write);
        } else {
            ret = snd_pcm_writei(ctx.pcm_handle, ctx.silence, samples_to_write);
        }
        if (ret < 0) {
            fprintf(stderr, "ALSA Error writei real: %s\n", snd_strerror(ret));
            snd_pcm_prepare(ctx.pcm_handle);
        } else {
            //std::cout << "    frames written real: " << ret << std::endl;
        }
    } else {
        // Underrrun
        ret = snd_pcm_writei(ctx.pcm_handle, ctx.silence, 512);
        if (ret < 0) {
            fprintf(stderr, "ALSA Error writei silence: %s\n", snd_strerror(ret));
            snd_pcm_prepare(ctx.pcm_handle);
        } else {
            std::cout << "Silence samples written: " << ret << std::endl;
        }
    }
}


static void alsa_worker(struct OutputState &output_state) {
    int                ret;
    snd_pcm_t         *pcm_handle = nullptr;
    struct pollfd     *poll_descs = nullptr;
    unsigned int       num_poll_descs = 0;
    unsigned short     revents;
    struct timeval     current_time;
    struct timeval     last_stat_time;
    struct OutputState ctx = output_state;

    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "Starting ALSA thread" << std::endl;
    cout_lock.clear();

    for (int i = 0; i < 2048; i++) {
        ctx.silence[i] = 0;
        ctx.audio_buffer_float[i] = 0.0f;
        ctx.audio_buffer_s16[i] = 0;
    }

    //
    // Look here for ALSA tips: http://equalarea.com/paul/alsa-audio.html
    //

    // Open the sound device
    pcm_handle = open_alsa_dev(ALSA_DEVICE);
    if (!pcm_handle) return;

    // Determine how many descriptos we need to poll for the ALSA device
    ret = snd_pcm_poll_descriptors_count(pcm_handle);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to get number of poll descriptors: " << snd_strerror(ret) << std::endl;
        close_alsa_dev(pcm_handle);
        return;
    }

    num_poll_descs = ret;
    std::cout << "Number of ALSA descriptors to poll: " << num_poll_descs << std::endl;

    // Allocate space and get the descriptors
    poll_descs = (struct pollfd*)calloc(sizeof(struct pollfd), num_poll_descs);
    ret = snd_pcm_poll_descriptors(pcm_handle, poll_descs, num_poll_descs);
    if (ret < 0) {
        std::cerr << "ALSA error. Unable to get poll descriptors: " << snd_strerror(ret) << std::endl;
    } else if ((unsigned int)ret != num_poll_descs) {
        std::cerr << "ALSA error. Number of filled descriptors (" << ret << ") differs from requested" << std::endl;
        num_poll_descs = (unsigned int)ret;
    }

    // See /usr/include/bits/poll.h
    static const std::vector<std::pair<int, std::string>> poll_events{
        {POLLIN, "POLLIN"},
        {POLLPRI, "POLLPRI"},
        {POLLOUT, "POLLOUT"},
        // __USE_XOPEN specific events omitted
        {POLLMSG, "POLLMSG"},
        {POLLREMOVE, "POLLREMOVE"},
        {POLLRDHUP, "POLLRDHUP"}
    };

    std::cout << "Will poll the following descriptors:" << std::endl;
    for (unsigned desc = 0; desc < num_poll_descs; desc++) {
        std::cout << "    " << poll_descs[desc].fd << " ";

        bool first_written = false;
        auto iter = poll_events.begin();
        while (iter != poll_events.end()) {
            if (poll_descs[desc].events & iter->first) {
                if (first_written) {
                    std::cout << ", ";
                } else {
                    std::cout << "(";
                    first_written =  true;
                }
                std::cout << iter->second;
            }
            ++iter;
        }

        std::cout << ")" << std::endl;
    }

    //FIR    flt(coeff_nbam_channel);
    FIR    flt(coeff_bpam_channel);
    //FIR     flt(coeff_12k5_channel);
    flt.setGain(ctx.settings.lf_gain);

    gettimeofday(&current_time, NULL);
    last_stat_time = current_time;

    ctx.pcm_handle     = pcm_handle;
    ctx.last_stat_time = last_stat_time;
    ctx.audio_filter   = &flt;
    ctx.fft_plan       = fftwf_plan_dft_1d(FFT_SIZE,
                                           reinterpret_cast<fftwf_complex*>(ctx.fft_in),
                                           reinterpret_cast<fftwf_complex*>(ctx.fft_out),
                                           FFTW_FORWARD, FFTW_ESTIMATE);
    ctx.sql_wait       = 0;
    ctx.fft_samples    = 0;
    ctx.sql_open       = false;
    ctx.energy_idx     = 0;
    ctx.hi_energy.reserve(10);
    ctx.lo_energy.reserve(10);
    ctx.agc.setAttack(1.0f);
    ctx.agc.setDecay(0.01f);
    ctx.agc.setReference(1.0f);

    // Create FFT window
    // Hamming: 0.54-0.46cos(2*pi*x/N), 0 <= n <= N. Length L = N+1
    for (unsigned n = 0; n <= FFT_SIZE; n++) {
        ctx.window[n] = 0.54f - 0.46f * std::cos((2.0f * M_PI * n) / FFT_SIZE);
    }

    // A bit unclear if this is needed since we configure the device for
    // "auto start" when initial amount of samples have been written.
    // Investigate futher...
    //snd_pcm_prepare(pcm_handle);

    // We can use ALSAs internal event loop instead of our own. Maybe ALSA
    // uses threads under the hood as well?

    while (!quit) {
        // Block until a descriptor indicates activity or 1s of inactivity
        ret = poll(poll_descs, num_poll_descs, 1000000);
        if (ret < 0) {
            std::cerr << "poll error. ret = " << ret << std::endl;
            continue;
        } else if (ret == 0) {
            // Inactivity timeout. Just poll again
            continue;
        }

        // One or more fd:s indicated activity. Loop over all fds watched
        // and check which have activiy (i.e. .revents != 0)
        for (unsigned desc = 0; desc < num_poll_descs; desc++) {
            if (poll_descs[desc].revents != 0) {
                // Need to translate the actual fd event to one that is
                // logical to ALSA
                ret = snd_pcm_poll_descriptors_revents(pcm_handle, &poll_descs[desc], 1, &revents);
                if (ret < 0) {
                    fprintf(stderr, "ALSA Error revents: %s\n", snd_strerror(ret));
                } else {
                    if (revents & POLLOUT) {
                        // The device indicate that it wants data to output
                        alsa_write_cb(ctx);
                    } else {
                        // Totally unexpected ALSA event in our program
                    }
                }
            }
        }
    }

    fftwf_destroy_plan(ctx.fft_plan);

    close_alsa_dev(pcm_handle);

    free(poll_descs);

    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "ALSA thread stopped" << std::endl;
    cout_lock.clear();
}


// Called when new IQ data is available
static void iq_cb(unsigned char *buf, uint32_t len, void *user_data) {
    struct InputState *ctx = reinterpret_cast<struct InputState*>(user_data);
    iqsample_t        *iq_buf_ptr;
    unsigned           decimated_out_len;

    if (quit) {
        rtlsdr_cancel_async(ctx->rtl_device);
        return;
    }

    //
    // We use a ADC scale from -1.0 to 1.0. To calculate dBFS we then just
    // use:
    //
    // dBFS = 10 * log10(i*i + q*q);
    //

    // Acquire IQ ring buffer, decimate and write the result to the buffer
    if (ctx->rb_ptr->acquireWrite(&iq_buf_ptr, DECIMATED_SIZE)) {
        ctx->ds->decimate(buf, len, iq_buf_ptr, &decimated_out_len);
        if (!ctx->rb_ptr->commitWrite(decimated_out_len)) {
            printf("iq_rb write commit error\n");
        }

        if (decimated_out_len != DECIMATED_SIZE) {
            printf("Major sample rate calculation error!\n");
        }

        ctx->i_levels[ctx->num_levels] = ctx->ds->iMean();
        ctx->q_levels[ctx->num_levels] = ctx->ds->qMean();
        if (++ctx->num_levels == LEVEL_SIZE) {
            float i_lvl=0.0f, q_lvl=0.0f;
            for (unsigned i = 0; i < LEVEL_SIZE; i++) {
                i_lvl += ctx->i_levels[i];
                q_lvl += ctx->q_levels[i];
            }

            i_lvl = i_lvl / LEVEL_SIZE;
            q_lvl = q_lvl / LEVEL_SIZE;

            //std::cout << "Level stats: i=" << i_lvl << ", q=" << q_lvl << std::endl;
            //printf("Level stats: i=%.4f, q=%.4f, overloaded=%s, cnt=%u, decimated_out_len=%u\n",
            //       i_lvl, q_lvl, decimator.overloaded() ? "true":"false", decimator.overloadCnt(), decimated_out_len);
            ctx->num_levels = 0;
        }
    } else {
        // Overrun
        printf("iq_rb buffer full\n");
    }

    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    bytes += len;
    samples += (len >> 1);
    frames++;

    // Output stats every STAT_INTERVAL seconds
    if (current_time.tv_sec > last_stat_calc.tv_sec + STAT_INTERVAL) {
        //double elapsed_time = (double)(current_time.tv_sec*1000000+current_time.tv_usec - (last_stat_calc.tv_sec*1000000+last_stat_calc.tv_usec));
        //double data_rate = ((double)bytes)/(elapsed_time/1000000);
        //double frame_rate = ((double)frames)/(elapsed_time/1000000);
        //double sample_rate = ((double)samples)/(elapsed_time/1000000);
        bytes = 0;
        frames = 0;
        samples = 0;
        last_stat_calc = current_time;

        //printf("len: %" PRIu32 ", Rate: %.4f Bps, %.2f fps, %.2f Sps\n", len, data_rate, frame_rate, sample_rate);
    }
}


static void dongle_worker(struct InputState &input_state) {
    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "Starting dongle thread for device: " << input_state.settings.rtl_device << std::endl;
    cout_lock.clear();

    while (!quit) {
        sleep(1);
    }

    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "Dongle thread stopped" << std::endl;
    cout_lock.clear();
}


// Parse a string with a frequency in MHz and with dot (.) as decimal separator
// into a frequency in Hz. If aeronautical is true, the parser expect a
// aeronautical 8.33 or 25 kHz channel number instead of a frequency.
//
// Returns the parsed frequency or 0 if a invalid string is given.
uint32_t parse_fq(const std::string &str, bool aeronautical = false) {
    uint32_t fq = 0;
    unsigned mhz = 0;
    unsigned hz = 0;

    // Dot is used as decimal separator
    auto dot_pos = str.find_first_of('.');

    // Decimal dot must be present
    if (dot_pos == std::string::npos) return 0;

    auto int_str  = str.substr(0, dot_pos); // integral part
    auto frac_str = str.substr(dot_pos+1);  // fractional part

    // Integral and fractional strings must contain digits and have correct lengths
    if (!std::all_of(int_str.begin(), int_str.end(), ::isdigit) ||
        !std::all_of(frac_str.begin(), frac_str.end(), ::isdigit) ||
        int_str.length() < 2 || int_str.length() > 4 ||
        frac_str.length() == 0 || frac_str.length() > 6
    ) return 0;

    // Aeronautical notation reqires exactly thre digits in the fractional part
    if (aeronautical && frac_str.length() != 3) return 0;

    if (aeronautical) {
        // A 100kHz wide band "contains" 12 8.33kHz channels or 4 25kHz
        // channels. Each channel has it's unique two digits making it
        // possible to correctly convert a channel in both schemas
        const std::map<std::string, unsigned> sub_ch_map{
            { "00",     0 }, { "05",     0 }, { "10",  8333 }, { "15", 16667 },
            { "25", 25000 }, { "30", 25000 }, { "35", 33333 }, { "40", 41667 },
            { "50", 50000 }, { "55", 50000 }, { "60", 58333 }, { "65", 66667 },
            { "75", 75000 }, { "80", 75000 }, { "85", 83333 }, { "90", 91667 }
        };
        auto sub_ch = sub_ch_map.find(frac_str.substr(1));
        if (sub_ch != sub_ch_map.end()) {
            mhz = std::atol(int_str.c_str());
            hz = (frac_str[0] - '0') * 100000 + sub_ch->second;
        }
    } else {
        mhz = std::atol(int_str.c_str());
        const std::vector<unsigned> frac_multipliers = { 100000, 10000, 1000, 100, 10, 1 };

        auto digit = frac_str.begin();
        auto multi = frac_multipliers.begin();
        while (digit != frac_str.end()) {
            hz += (*digit - '0') * *multi;
            ++digit;
            ++multi;
        }
    }

    if (mhz < 4000) {
        fq = mhz * 1000000 + hz;
    }

    return fq;
}


// Parse the command line and fill the settings object. Return 0 on success
// or < 0 on parse error or if help was requested
static int parse_cmd_line(int argc, char **argv, class Settings &settings) {
    int           ret = -1;
    poptContext   popt_ctx;
    char         *audio_device = nullptr;
    int           print_help = 0;
    int           normal_fq_fmt = 0;

    struct poptOption options_table[] = {
        { "rtl-dev",   'd', POPT_ARG_INT,    &settings.rtl_device, 0, "RTL-SDR device ID to use. Defaults to 0 if not set", "RTLDEVEID" },
        { "fq-corr",   'c', POPT_ARG_INT,    &settings.fq_corr, 0, "frequency correction in ppm. Defaults to 0 if not set", "FQCORR" },
        { "rf-gain",   'r', POPT_ARG_FLOAT,  &settings.rf_gain, 0, "RF gain in dB in the range 0 to 49. Defaults to 30 if not set", "RFGAIN" },
        { "lf-gain",   'l', POPT_ARG_FLOAT,  &settings.lf_gain, 0, "audio gain in dB. Defaults to 3 if not set", "LFGAIN" },
        { "sql-level", 's', POPT_ARG_FLOAT,  &settings.sql_level, 0, "squelch level in dB over noise. Defaults to 15 if not set", "SQLLEVEL" },
        { "audio-dev",   0, POPT_ARG_STRING, &audio_device, 0, "ALSA audio device string. Defaults to 'default' if not set", "AUDIODEV" },
        { "fq-mode",     0, POPT_ARG_NONE,   &normal_fq_fmt, 0, "interpret the CHANNEL argument as a normal frequency in MHz", nullptr },
        { "help",      'h', POPT_ARG_NONE,   &print_help, 0, "show this help message and quit", nullptr },
        POPT_TABLEEND
    };

    popt_ctx = poptGetContext(nullptr, argc, (const char**)argv, options_table, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(popt_ctx, "[OPTION...] CHANNEL");

    // Parse options
    while ((ret = poptGetNextOpt(popt_ctx)) > 0);
    if (ret < -1) {
        // Error while parsing the options. Print the reason
        switch (ret) {
            case POPT_ERROR_BADOPT:
                std::cerr << "Error: Unknown option given" << std::endl;
                break;
            case POPT_ERROR_NOARG:
                std::cerr << "Error: Missing option value" << std::endl;
                break;
            case POPT_ERROR_BADNUMBER:
            case POPT_ERROR_OVERFLOW:
                std::cerr << "Error: Option could not be converted to number" << std::endl;
                break;
            default:
                std::cerr << "Error: Unknown error in option parsing: " << ret << std::endl;
                break;
        }
        poptPrintHelp(popt_ctx, stderr, 0);
        ret = -1;
    } else {
        // Options parsed without error
        ret = 0;

        if (print_help) {
            // Ignore given options and just print the extended help
            poptPrintHelp(popt_ctx, stderr, 0);
            std::cerr << R"(
sdrx is a simple software defined narrow band AM receiver using a RTL-SDR
dongle as its hadware backend. It is mainly designed for use in the 118 to
138 MHz airband. The program is run from the command line and the channel to
listen to is given as an argument in standard six digit aeronautical notation.
Both the legacy 25kHz channel separation and the new 8.33kHz channel separation
notations are supported, i.e. 118.275 and 118.280 both mean the frequency
118.275 MHz.

The squelch is adaptive with respect to the current noise floor and the squelch
level is given as a SNR value in dB. Audio is played using ALSA.

The --fq-mode option (with no argument) can be used to give the frequency in MHz
instead of as a channel number.

Examples:

Listen to the channel 122.450 with 40dB of RF gain and 8dB of audio gain:

    $ sdrx --rf-gain 40 --lf-gain 8 122.450

Listen to the channel 118.105 with 34dB of RF gain, 12dB of audio gain and
18dB squelch:

    $ sdrx --rf-gain 34 --lf-gain 12 --sql-level 18 118.105

Listen to the frequency 118.111003 MHz:

    $ sdrx --fq-mode 118.111003
)"
            << std::endl;
            ret = -1;
        } else {
            // Save audio device if it was given
            if (audio_device) {
                settings.audio_device = audio_device;
            }

            // Check that the given option values are within range
            if (settings.rtl_device < 0) {
                std::cerr << "Error: Invalid RTL-SDR device ID given: " << settings.rtl_device << std::endl;
                ret = -1;
            }
            if (settings.rf_gain < 0.0f || settings.rf_gain > 50.0f) {
                fprintf(stderr, "Error: Invalid RF gain given: %.4f\n", settings.rf_gain);
                ret = -1;
            }
            if (settings.sql_level < 0.0f || settings.sql_level > 50.0f) {
                fprintf(stderr, "Error: Invalid SQL level given: %.4f\n", settings.sql_level);
                ret = -1;
            }

            if (poptPeekArg(popt_ctx) != nullptr) {
                const char *arg = poptGetArg(popt_ctx);

                bool fq_type = AERONAUTICAL_CHANNEL;
                if (normal_fq_fmt) fq_type = NORMAL_FQ;

                settings.channel = arg;
                settings.fq = parse_fq(arg, fq_type);
                if (settings.fq == 0) {
                    std::cerr << "Error: Invalid " << (fq_type == NORMAL_FQ ? "frequency":"channel") << " given: " << arg << std::endl;
                    ret = -1;
                } else if (settings.fq < 45000000 || settings.fq > 1800000000) {
                    fprintf(stderr, "Error: Invalid frequency given: %" PRIu32 "Hz\n", settings.fq);
                    ret = -1;
                }
            } else {
                std::cerr << "Error: No channel/frequency given" << std::endl;
                ret = -1;
            }

            if (ret < 0) {
                poptPrintHelp(popt_ctx, stderr, 0);
            }
        }
    }

    poptFreeContext(popt_ctx);

    return ret;
}


int main(int argc, char** argv) {
    int              ret;
    struct sigaction sigact;
    Settings         settings;

    // Parse command line. Exit if incomplete or if help was requested
    if (parse_cmd_line(argc, argv, settings) < 0) {
        return 1;
    }

    /*
    // Print the settings
    std::cout << "The folowing settings were given:" << std::endl;
    std::cout << "    RTL device: " << settings.rtl_device << std::endl;
    std::cout << "    Frequency correction: " << settings.fq_corr << "ppm" << std::endl;
    std::cout << "    RF gain: " << settings.rf_gain << "dB" << std::endl;
    std::cout << "    LF (audio) gain: " << settings.lf_gain << "dB" << std::endl;
    std::cout << "    Squelch level: " << settings.sql_level << "dB" << std::endl;
    std::cout << "    ALSA device: " << settings.audio_device << std::endl;
    std::cout << "    Frequency: " << settings.fq << "Hz" << std::endl;
    */

    MSD downsampler(std::vector<MSD::Stage>{
        {3, lp_ds_1200k_400k},
        {5, lp_ds_400k_80k},
        {5, lp_ds_80k_16k}
    });
    RB<iqsample_t> iq_rb(256*16); // 16 chunks or 256ms

    // Mute librtlsdr internal fprintf(stderr, "..."); Will mute our own printouts as well...
    //freopen("/dev/null", "w", stderr);

    struct InputState input_state;
    input_state.settings   = settings;
    input_state.rtl_device = nullptr;
    input_state.ds         = &downsampler;
    input_state.rb_ptr     = &iq_rb;
    input_state.num_levels = 0;

    ret = rtlsdr_open(&input_state.rtl_device, settings.rtl_device);
    if (ret < 0 || input_state.rtl_device == NULL) {
        std::cerr << "Error: Unable to open device " << settings.rtl_device << ". ret = " << ret << std::endl;
        return 1;
    }

    std::cout << "RTL-SDR device " << settings.rtl_device << " opend" << std::endl;

    uint32_t rtl2832_clk_fq;
    uint32_t tuner_clk_fq;
    rtlsdr_get_xtal_freq(input_state.rtl_device, &rtl2832_clk_fq, &tuner_clk_fq);
    std::cout << "RTL2832 clock Fq: " << rtl2832_clk_fq << "Hz" << std::endl;
    std::cout << "Tuner clock Fq: " << tuner_clk_fq << "Hz" << std::endl;

    enum rtlsdr_tuner tuner_type = rtlsdr_get_tuner_type(input_state.rtl_device);
    std::cout << "Tuner type: " << rtlsdr_tuner_to_str(tuner_type) << std::endl;

    int num_tuner_gains = rtlsdr_get_tuner_gains(input_state.rtl_device, NULL);
    int *tuner_gains = (int*)calloc(sizeof(int), num_tuner_gains);

    rtlsdr_get_tuner_gains(input_state.rtl_device, tuner_gains);
    std::cout << "Tuner gain steps (dB):";
    for (int i = 0; i < num_tuner_gains; i++) {
        fprintf(stdout, " %.1f", ((float)tuner_gains[i])/10.0f);
    }
    std::cout << std::endl;
    free(tuner_gains);
    tuner_gains = nullptr;

    uint32_t sample_rate = 1200000;
    uint32_t bw = 300000;

    std::cout << "Setting up device with ";
    std::cout << "Fq=" << settings.fq << "Hz, ";
    std::cout << "Corr=" << settings.fq_corr << "ppm, ";
    std::cout << "Bw=" << bw << "Hz, ";
    std::cout << "Gain=" << settings.rf_gain << "dB, ";
    std::cout << "Sample rate=" << sample_rate << " samples/s";
    std::cout << std::endl;

    ret = rtlsdr_set_center_freq(input_state.rtl_device, settings.fq);
    ret = rtlsdr_set_freq_correction(input_state.rtl_device, settings.fq_corr);
    ret = rtlsdr_set_tuner_bandwidth(input_state.rtl_device, bw);
    ret = rtlsdr_set_tuner_gain(input_state.rtl_device, settings.rf_gain * 10);
    ret = rtlsdr_set_sample_rate(input_state.rtl_device, sample_rate);

    // Check what happend
    uint32_t actual_sample_rate = rtlsdr_get_sample_rate(input_state.rtl_device);
    if (actual_sample_rate != sample_rate) {
        std::cerr << "Warning: Actual Sample rate: " << actual_sample_rate << "samples/s. Program will not work." << std::endl;
    }

    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);
    quit = 0;

    struct OutputState output_state;
    output_state.rb_ptr   = &iq_rb;
    output_state.settings = settings;

    std::thread alsa_thread(alsa_worker, std::ref(output_state));
    std::thread dongle_thread(dongle_worker, std::ref(input_state));

    bytes = 0;
    samples = 0;
    frames = 0;
    gettimeofday(&last_stat_calc, NULL);
    rtlsdr_reset_buffer(input_state.rtl_device);

    // IQ_BUF_SIZE is the size of the callback buffer. This will become len
    // in the data callback.
    ret = rtlsdr_read_async(input_state.rtl_device, iq_cb, &input_state, 4, IQ_BUF_SIZE);
    if (ret < 0) {
        std::cerr << "Error: rtlsdr_read_async returned: " << ret << std::endl;
        return(0);
    }

    while (!quit) {
        sleep(1);
    }

    dongle_thread.join();
    alsa_thread.join();

    std::cout << "Closing RTL device." << std::endl;
    rtlsdr_close(input_state.rtl_device);

    std::cout << "Stopped." << std::endl;
}
