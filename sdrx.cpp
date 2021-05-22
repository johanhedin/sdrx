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
#include <time.h>

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
#include "crb.hpp"
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


// Sampling, down sampling, latency, audio and IQ buffer sizes
//
// A sample frequency that is evenly divided with the TCXO clock is advicable.
// The dongle clock runs at 28.8MHz. This gives "nice" IQ sampling frequencies
// of 2 400 000 (1/12th), 1 800 000 (1/16th) 1 200 000 (1/24th),
// 960 000 (1/30th) and so on.
//
// To extract the channel of intrest (16kHz bandwidth) we need to down sample
// with an integral factor.
//
// The minumum "block" of data that can be transported through USB is 512 bytes
// so the buffer size used when starting librtlsdr must be a multiple of 512
// bytes. 512 bytes equals 256 complex samples and represent a "time slice"
// of (512/2)/fs seconds.
//
// Since batches of just 512 bytes are inefficient we need to have a larger
// buffer. Larger buffer will also give us samples in batches that nicely
// match the sound card requirements.
//
// Example:
//
// With a IQ sampling frequency of 1200000 and a target channel sampling
// frequency of 16000Hz we need to down sample by a factor of 75 (1200/16).
//
// We like to work with chunks of channel IQ data of 512 samples (@16kHz) which
// corresponds to 32ms. To get 512 samples after the down sampler we need to
// have 512*75 samples before the down sampler. Since one sample occupy two
// bytes from the dongle we need to set the librtlsdr buffer size to 512*75*2.
//
#define RTL_IQ_SAMPLING_FQ   1200000
#define MAX_CH_OFFSET        500000   // 500Khz
#define DOWNSAMPLING_FACTOR  75
#define RTL_IQ_BUF_SIZE      (512 * DOWNSAMPLING_FACTOR * 2)  // callback frequency is 31.25Hz or 32ms
#define CH_IQ_SAMPLING_FQ    16000    // RTL_IQ_SAMPLING_FQ / DOWNSAMPLING_FACTOR
#define CH_IQ_BUF_SIZE       512
#define FFT_SIZE             CH_IQ_BUF_SIZE

static bool run = true;
static std::atomic_flag cout_lock = ATOMIC_FLAG_INIT;

// Metadata associated with one chunk of IQ data (32ms at the moment)
struct Metadata {
    struct timeval ts;       // Timestamp for the IQ chunk (from the OS)
    float          pwr_dbfs; // Power dBFS (ref. full scale sine wave)
};

// Convenient type for the ring buffer
using rb_t = CRB<iqsample_t, Metadata>;

using sql_state_t = enum { SQL_CLOSED, SQL_OPEN };

// Datatype to represent one channel in the IQ spectra
class Channel {
public:
    Channel(const std::string &name, float sql_level = 9.0f)
    : name(name), sql_level(sql_level), sql_state(SQL_CLOSED), sql_state_prev(SQL_CLOSED), pos(0) {}

    bool operator<(const Channel &rhv) { return name < rhv.name; }
    bool operator==(const Channel &rhv) { return name == rhv.name; }

    bool operator<(const char *rhv) { return name < rhv; }
    bool operator==(const char *rhv) { return name == rhv; }

    std::string    name;           // Channel name, e.g "118.105"
    MSD            msd;            // Multi stage down sampler with tuner
    AGC            agc;            // AGC
    float          sql_level;      // Squelch level for the channel
    sql_state_t    sql_state;      // Squelch state (open/closed)
    sql_state_t    sql_state_prev; // Previous squelch state (open/closed)
    int            pos;            // Audio position. 0 == center
};


// Datatype to hold the sdrx settins
class Settings {
public:
    Settings(void) : rtl_device(0), fq_corr(0), rf_gain(30.0f), lf_gain(0.0f),
                     sql_level(9.0f), audio_device("default"), tuner_fq(0)
                     {}
    int         rtl_device;   // RTL-SDR device to use (in id form)
    int         fq_corr;      // Frequency correction in ppm
    float       rf_gain;      // RF gain in dB
    float       lf_gain;      // LF (audio gain) in dB
    float       sql_level;    // Squelch level in dB (over noise level)
    std::string audio_device; // ALSA device to use for playback
    uint32_t    tuner_fq;     // Tuner frequency
    std::vector<Channel> channels;     // String representations of the channels to listen to
};


#define LEVEL_SIZE 10
struct InputState {
    rtlsdr_dev_t   *rtl_device;              // RTL device
    rb_t           *rb_ptr;                  // Input -> Output buffer
    Settings        settings;                // System wide settings
    unsigned char   iq_buf[RTL_IQ_BUF_SIZE]; // Jump buffer to handle Pi4 with 5.x kernel
};


struct OutputState {
    snd_pcm_t         *pcm_handle;               // ALSA PCM device
    rb_t              *rb_ptr;                   // Input -> Output buffer
    int16_t            silence[CH_IQ_BUF_SIZE*2];            // Stereo
    float              audio_buffer_float[CH_IQ_BUF_SIZE*2]; // Stereo
    int16_t            audio_buffer_s16[CH_IQ_BUF_SIZE*2];   // Stereo
    FIR2              *audio_filter;
    iqsample_t         fft_in[FFT_SIZE];
    iqsample_t         fft_out[FFT_SIZE];
    float              window[FFT_SIZE+1];
    fftwf_plan         fft_plan;
    unsigned           sql_wait;
    std::vector<float> hi_energy;
    std::vector<float> lo_energy;
    unsigned           energy_idx;
    Settings           settings;
};


static void signal_handler(int signo) {
    std::cout << "Signal '" << strsignal(signo) << "' received. Stopping...\n";
    run = false;
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


// Called by librtlsdr when a chunk of new IQ data is available
static void iq_cb(unsigned char *buf, uint32_t len, void *user_data) {
    struct InputState    &ctx = *reinterpret_cast<struct InputState*>(user_data);
    iqsample_t           *iq_buf_ptr = nullptr;
    std::vector<Channel> &channels = ctx.settings.channels;
    struct Metadata      *metadata_ptr = nullptr;
    struct Metadata       meta;

    if (len != RTL_IQ_BUF_SIZE) {
        std::cerr << "Error: Unexpected number of IQ bytes received from dongle: " <<
                     len << ". " << RTL_IQ_BUF_SIZE << " expected." << std::endl;
        return;
    }

    if (!run) {
        rtlsdr_cancel_async(ctx.rtl_device);
        return;
    }

    // Copy to tmp buf to overcome slow access to data on Pi with kernel 5.x
    memcpy(ctx.iq_buf, buf, RTL_IQ_BUF_SIZE);

    // Prepare chunk metadata
    gettimeofday(&meta.ts, nullptr);
    float pwr_rms = 0.0f;

    // Calculate average power in the chunk by squaring the amplitude RMS.
    // ampl_rms = sqrt( ( sum( abs(iq_sample)^2 ) ) / N )
    for (unsigned i = 0; i < RTL_IQ_BUF_SIZE; i += 2) {
        iqsample_t iq_sample(((float)ctx.iq_buf[i])   / 127.5f - 1.0f, ((float)ctx.iq_buf[i+1])   / 127.5f - 1.0f);
        float ampl_squared = std::norm(iq_sample);
        pwr_rms += ampl_squared;
    }
    pwr_rms = pwr_rms / (RTL_IQ_BUF_SIZE/2);

    // Calculate power dBFS with full scale sine wave as reference (amplitude
    // of 1/sqrt(2) or power 1/2 or -3 dB).
    meta.pwr_dbfs = 10 * std::log10(pwr_rms) - 3.0f;

    // Acquire IQ ring buffer
    if (ctx.rb_ptr->acquireWrite(&iq_buf_ptr, &metadata_ptr)) {
        // Channelize the IQ data and write output into ring buffer
        for (auto &ch : channels) {
            ch.msd.decimate(ctx.iq_buf, len, iq_buf_ptr);
            iq_buf_ptr += CH_IQ_BUF_SIZE;
        }

        // Store IQ metadata in the chunk
        *metadata_ptr = meta;

        if (!ctx.rb_ptr->commitWrite()) {
            std::cerr << "Error: Unable to commit ring buffer write" << std::endl;
        }
    } else {
        // Overrun
        std::cerr << "Warning: Unable to write samples. Ring buffer full" << std::endl;
    }
}


// Render a dBFS level for a IQ sample as a ASCII bargraph
static void render_bargraph(float level, char *buf) {
    int lvl = (int)level;

    if (lvl < -56) lvl = -56;
    if (lvl > 0) lvl = 0;

    int tmp_level = lvl + 56;
    int base = tmp_level/8;
    int rest = tmp_level - base * 8;

    snprintf(buf, 65, "\033[32m"); // Green
    buf += 5;
    for (int i = 0; i < 7; i++) {
        if (i == 5) {
            snprintf(buf, 65, "\033[33m"); // Yellow(/brown)
            buf += 5;
        }
        if (i == 6) {
            snprintf(buf, 65, "\033[31m"); // Red
            buf += 5;
        }

        if (i < base) {
            snprintf(buf, 65, "\u2588");
            buf += 3;
        } else if (i == base) {
            if (rest == 0) {
                snprintf(buf, 65, " ");
                buf += 1;
            } else {
                switch (rest) {
                    case 1: snprintf(buf, 9, "\u258f"); break;
                    case 2: snprintf(buf, 9, "\u258e"); break;
                    case 3: snprintf(buf, 9, "\u258d"); break;
                    case 4: snprintf(buf, 9, "\u258c"); break;
                    case 5: snprintf(buf, 9, "\u258b"); break;
                    case 6: snprintf(buf, 9, "\u258a"); break;
                    case 7: snprintf(buf, 9, "\u2589"); break;
                }
                buf += 3;
            }
        } else {
            snprintf(buf, 65, " ");
            buf += 1;
        }
    }
    snprintf(buf, 65, "\033[0m");
}


// Called when the sound card wants another period, i.e. every 32 ms
static void alsa_write_cb(OutputState &ctx) {
    int                    ret;
    const iqsample_t      *iq_buffer;
    const struct Metadata *metadata_ptr = nullptr;
    struct tm              tm;
    char                   tmp_str[256];
    char                   bar[71];
    std::vector<Channel>  &channels = ctx.settings.channels;

    ret = snd_pcm_avail_update(ctx.pcm_handle);
    if (ret < 0) {
        std::cerr << "ALSA Error pcm_avail: " << snd_strerror(ret) << std::endl;
        //snd_pcm_prepare(ctx.pcm_handle);
        // What to do here? Restart device? Just return?
    }

    if (ctx.rb_ptr->acquireRead(&iq_buffer, &metadata_ptr)) {
        if (ctx.sql_wait >= 10) {
            struct timeval current_time;
            gettimeofday(&current_time, NULL);
            localtime_r(&current_time.tv_sec, &tm);
            strftime(tmp_str, 100, "%T", &tm);
            render_bargraph(metadata_ptr->pwr_dbfs, bar);
            fprintf(stdout, "%s: Level[%s\033[1;30m%5.1f\033[0m]", tmp_str, bar, metadata_ptr->pwr_dbfs);
        }

        // Zero out the output audio buffer
        memset(ctx.audio_buffer_float, 0, sizeof(ctx.audio_buffer_float));
        unsigned j = 0;
        for (auto &ch : channels) {
            for (unsigned i = 0; i < CH_IQ_BUF_SIZE; ++i) {
                // Should we always run IQ samples through the AGC even if the squelsh is not open?
                iqsample_t agc_adj_sample = ch.agc.adjust(iq_buffer[j]); // AGC adjusted IQ sample
                if (ch.sql_state == SQL_OPEN) {
                    // AM demodulator
                    float s = std::abs(agc_adj_sample);

                    // If the squelsh has just opend, ramp up the audio
                    if (ch.sql_state_prev == SQL_CLOSED) {
                        s = ramp_up[i] * s;
                    }

                    // Mix this channel into the output buffer
                    switch (ch.pos) {
                        case -2:
                            ctx.audio_buffer_float[i*2] += 0.8f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.2f*s;
                            break;

                        case -1:
                            ctx.audio_buffer_float[i*2] += 0.6f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.4f*s;
                            break;

                        case 1:
                            ctx.audio_buffer_float[i*2] += 0.4f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.6f*s;
                            break;

                        case 2:
                            ctx.audio_buffer_float[i*2] += 0.2f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.8f*s;
                            break;

                        default:
                            // Center
                            ctx.audio_buffer_float[i*2] += 0.5f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.5f*s;
                            break;
                    }
                } else if (ch.sql_state_prev == SQL_OPEN) {
                    // Ramp down
                    float s = std::abs(agc_adj_sample);
                    s = ramp_down[i] * s;

                    // Mix this channel into the output buffer
                    switch (ch.pos) {
                        case -2:
                            ctx.audio_buffer_float[i*2] += 0.8f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.2f*s;
                            break;

                        case -1:
                            ctx.audio_buffer_float[i*2] += 0.6f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.4f*s;
                            break;

                        case 1:
                            ctx.audio_buffer_float[i*2] += 0.4f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.6f*s;
                            break;

                        case 2:
                            ctx.audio_buffer_float[i*2] += 0.2f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.8f*s;
                            break;

                        default:
                            // Center
                            ctx.audio_buffer_float[i*2] += 0.5f*s;
                            ctx.audio_buffer_float[i*2+1] += 0.5f*s;
                            break;
                    }
                }
                ctx.fft_in[i] = iq_buffer[j] * ctx.window[i];  // Fill fft buffer
                ++j;
            }
            ch.sql_state_prev = ch.sql_state;

            fftwf_execute(ctx.fft_plan);
            // **** Calculate sql for channel here. Start

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
                ref_level_hi += std::norm(ctx.fft_out[i] * passband_shape[i]);
                ref_level_lo += std::norm(ctx.fft_out[FFT_SIZE - i] * passband_shape[FFT_SIZE - i]);
                //ref_level_hi += std::norm(ctx.fft_out[i]);
                //ref_level_lo += std::norm(ctx.fft_out[FFT_SIZE - i]);
            }
            ref_level_hi /= 45;
            ref_level_lo /= 45;
            float noise_level = (ref_level_hi + ref_level_lo) / 2;

            // Calculate SNR
            float snr = 10 * std::log10(sig_level / noise_level);
            if (snr > ch.sql_level) {
                ch.sql_state = SQL_OPEN;
            } else {
                ch.sql_state = SQL_CLOSED;
            }

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

            // Convert levels to dB. Division by 512 is for compensating for
            // the FFT gain (number of points, N)
            sig_level    = 10 * std::log10(sig_level/512.0f);
            ref_level_hi = 10 * std::log10(ref_level_hi/512.0f);
            ref_level_lo = 10 * std::log10(ref_level_lo/512.0f);

            if (ctx.sql_wait >= 10) {
                lo_energy = 0.0f;
                hi_energy = 0.0f;
                for (unsigned i = 0; i < 10; ++i) {
                    lo_energy += ctx.lo_energy[i];
                    hi_energy += ctx.hi_energy[i];
                }

                lo_energy /= 10;
                hi_energy /= 10;

                float imbalance = hi_energy - lo_energy;

                if (channels.size() == 1) {
                    if (ch.sql_state == SQL_OPEN) {
                        fprintf(stdout, "  \033[103m\033[30m%s\033[0m[\033[1;30m%4.1f|%5.1f|%5.1f|%5.1f|%6.2f\033[0m] (SNR|low|mid|hig|imbalance)",
                                ch.name.c_str(), snr, ref_level_lo, sig_level, ref_level_hi, imbalance);
                    } else {
                        fprintf(stdout, "  %s[\033[1;30m%4.1f|%5.1f|%5.1f|%5.1f|%6.2f\033[0m] (SNR|low|mid|hig|imbalance)",
                                ch.name.c_str(), snr, ref_level_lo, sig_level, ref_level_hi, imbalance);
                    }
                } else {
                    if (snr < 1.0f) snr = 0.0f;
                    if (ch.sql_state == SQL_OPEN) {
                        fprintf(stdout, "  \033[103m\033[30m%s\033[0m[\033[1;30m%4.1f\033[0m]", ch.name.c_str(), snr);
                    } else {
                        fprintf(stdout, "  %s[\033[1;30m%4.1f\033[0m]", ch.name.c_str(), snr);
                    }
                }
            }
            // **** Calculate sql for channel here. End
        }

        ctx.rb_ptr->commitRead();

        if (++ctx.sql_wait > 10) {
            ctx.sql_wait = 0;
            fprintf(stdout, "\n");
        }

        // Common filter for the mixed audio from all channels
        ctx.audio_filter->filter(ctx.audio_buffer_float, CH_IQ_BUF_SIZE*2, ctx.audio_buffer_float);

        // Convert float to 16 bit signed
        for (unsigned i = 0; i < CH_IQ_BUF_SIZE*2; ++i) {
            int16_t s;
            if (ctx.audio_buffer_float[i] > 1.0f)       s = 32767;
            else if (ctx.audio_buffer_float[i] < -1.0f) s = -32767;
            else s = (int16_t)(ctx.audio_buffer_float[i] * 32767.0f);

            ctx.audio_buffer_s16[i] = s;
        }

        // Write to sound card
        ret = snd_pcm_writei(ctx.pcm_handle, ctx.audio_buffer_s16, CH_IQ_BUF_SIZE);
        if (ret < 0) {
            std::cerr << "Error: Failed to write audio samples: " << snd_strerror(ret) << std::endl;
            snd_pcm_prepare(ctx.pcm_handle);
        } else {
            //std::cout << "    frames written real: " << ret << std::endl;
        }

    } else {
        // Underrrun
        std::cerr << "Warning: Unable to read samples. Ring buffer empty. Writing " << CH_IQ_BUF_SIZE << " samples of silence" << std::endl;
        ret = snd_pcm_writei(ctx.pcm_handle, ctx.silence, CH_IQ_BUF_SIZE);
        if (ret < 0) {
            std::cerr << "Error: Failed to write underrun silence: " << snd_strerror(ret) << std::endl;
            snd_pcm_prepare(ctx.pcm_handle);
        }
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
    num_channels        = 2;                     // "Stereo"
    pcm_period          = 512;                   // Play chunks of this many frames (512/16000 -> 32ms)
    pcm_buffer_size     = pcm_period * 8;        // Playback buffer is 8 periods, i.e. 256ms
    pcm_note_threshold  = pcm_period;            // Notify us when we can write at least this number of frames
    pcm_start_threshold = pcm_period * 4;        // Start playing when this many frames have been written to the device buffer

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


static void alsa_worker(struct OutputState &output_state) {
    int                ret;
    snd_pcm_t         *pcm_handle = nullptr;
    struct pollfd     *poll_descs = nullptr;
    unsigned int       num_poll_descs = 0;
    unsigned short     revents;
    struct timeval     current_time;
    struct OutputState ctx = output_state;

    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "Starting ALSA thread" << std::endl;
    cout_lock.clear();

    for (int i = 0; i < CH_IQ_BUF_SIZE*2; i++) {  // Stereo
        ctx.silence[i] = 0;
        ctx.audio_buffer_float[i] = 0.0f;
        ctx.audio_buffer_s16[i] = 0;
    }

    //
    // Look here for ALSA tips: http://equalarea.com/paul/alsa-audio.html
    //

    // Open the sound device
    pcm_handle = open_alsa_dev(ctx.settings.audio_device);
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

    FIR2 flt(coeff_bp4am_channel);
    flt.setGain(ctx.settings.lf_gain);

    gettimeofday(&current_time, NULL);

    ctx.pcm_handle     = pcm_handle;
    ctx.audio_filter   = &flt;
    ctx.fft_plan       = fftwf_plan_dft_1d(FFT_SIZE,
                                           reinterpret_cast<fftwf_complex*>(ctx.fft_in),
                                           reinterpret_cast<fftwf_complex*>(ctx.fft_out),
                                           FFTW_FORWARD, FFTW_ESTIMATE);
    ctx.sql_wait       = 0;
    ctx.energy_idx     = 0;
    ctx.hi_energy.reserve(10);
    ctx.lo_energy.reserve(10);

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

    while (run) {
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
                    std::cerr << "ALSA Error revents: " << snd_strerror(ret) << std::endl;
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


static void dongle_worker(struct InputState &input_state) {
    while (cout_lock.test_and_set(std::memory_order_acquire));
    std::cout << "Starting dongle thread for device: " << input_state.settings.rtl_device << std::endl;
    cout_lock.clear();

    while (run) {
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


// Get audio position for a channel given a channel index and total number of
// channels. Number of audio positions must be odd, typically 3 or 5
static int get_audio_pos(unsigned channel_no, unsigned num_channels) {
    unsigned num_positions = 5;
    unsigned half = num_channels / 2;
    unsigned odd = (num_channels % 2) == 0 ? 0 : 1;
    float    tmp;
    int      pos = 0;

    if (channel_no < num_channels) {
        if (channel_no < half) {
            tmp = (float)(channel_no * num_positions) / (float)num_channels;
            pos = floorf(tmp) - num_positions/2;
        } else if (channel_no == half && odd) {
            pos = 0;
        } else {
            tmp = (float)((num_channels-1 - channel_no)*num_positions) / (float)num_channels;
            pos = num_positions/2 - floorf(tmp);
        }
    }

    return pos;
}


// Parses the command line and fills the settings object. Checks that the
// options and arguments are within limits. Returns 0 on success or < 0 on
// parse/value error or if help was requested
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
        { "lf-gain",   'l', POPT_ARG_FLOAT,  &settings.lf_gain, 0, "audio gain in dB. Defaults to 0 if not set", "LFGAIN" },
        { "sql-level", 's', POPT_ARG_FLOAT,  &settings.sql_level, 0, "squelch level in dB over noise. Defaults to 9 if not set", "SQLLEVEL" },
        { "audio-dev",   0, POPT_ARG_STRING, &audio_device, 0, "ALSA audio device string. Defaults to 'default' if not set", "AUDIODEV" },
        { "help",      'h', POPT_ARG_NONE,   &print_help, 0, "show full help and quit", nullptr },
        POPT_TABLEEND
    };

    popt_ctx = poptGetContext(nullptr, argc, (const char**)argv, options_table, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(popt_ctx, "[OPTION...] CHANNEL [CHANNEL...]");

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
sdrx is a software defined narrow band AM receiver that is using a RTL-SDR
dongle as its hadware backend. It is mainly designed for use in the 118 to
138 MHz airband. The program is run from the command line and the channels to
listen to are given as arguments in the standard six digit aeronautical
notation. Both the legacy 25kHz channel separation and the new 8.33kHz channel
separation notations are supported, i.e. 118.275 and 118.280 both mean the
frequency 118.275 MHz.

If multiple channels are given, they must all fit within 1MHz of bandwidth.

The squelch is adaptive with respect to the current per channel noise floor and
the squelch level is given as a SNR value in dB. Audio is played using ALSA.

Audio gain and squelch can normally be left as is since the defaults work well.

Examples:

Listen to the channel 122.450 with 40dB of RF gain and 3dB of audio gain:

    $ sdrx --rf-gain 40 --lf-gain 8 122.450

Listen to the channels 118.105 and 118.505 with 34dB of RF gain and 5dB squelch:

    $ sdrx --rf-gain 34 --sql-level 5 118.105 118.505
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
            if (settings.sql_level < -10.0f || settings.sql_level > 50.0f) {
                fprintf(stderr, "Error: Invalid SQL level given: %.4f\n", settings.sql_level);
                ret = -1;
            }

            // Parse the arguments as channels
            if (poptPeekArg(popt_ctx) != nullptr) {
                bool fq_type = normal_fq_fmt ? NORMAL_FQ : AERONAUTICAL_CHANNEL;
                const char *arg;
                while ((arg = poptGetArg(popt_ctx)) != nullptr) {
                    uint32_t fq_ret = parse_fq(arg, fq_type);
                    if (fq_ret == 0) {
                        std::cerr << "Error: Invalid " << (fq_type == NORMAL_FQ ? "frequency":"channel") <<
                                     " given: " << arg << std::endl;
                        ret = -1;
                    } else if (fq_ret < 45000000 || fq_ret > 1800000000) {
                        std::cerr << "Error: Invalid frequency given: " << fq_ret << "Hz" << std::endl;
                        ret = -1;
                    } else {
                        // Add to channels list if not already present
                        if (std::find(settings.channels.begin(), settings.channels.end(), arg) == settings.channels.end()) {
                            settings.channels.push_back(Channel(arg, settings.sql_level));
                        }
                    }
                }

                if (settings.channels.size() > 0) {
                    if (settings.channels.size() > 1 && fq_type == NORMAL_FQ) {
                        std::cerr << "Error: Only one frequency allowed in frequency mode" << std::endl;
                        ret = -1;
                    } else {
                        // If here, we know that all channels in settings.channels
                        // are valid aeronautical and that we have at least one
                        // channel. Sort the vector and determine what tuner
                        // frequency to use (truncated to nearest 100kHz)
                        std::vector<Channel> tmp_channels = settings.channels;
                        std::sort(tmp_channels.begin(), tmp_channels.end());
                        std::string lo_ch = (tmp_channels.begin())->name;
                        std::string hi_ch = (--tmp_channels.end())->name;

                        uint32_t lo_fq = parse_fq(lo_ch, fq_type);
                        uint32_t hi_fq = parse_fq(hi_ch, fq_type);
                        uint32_t mid_fq = lo_fq + (hi_fq - lo_fq) / 2;
                        uint32_t mid_fq_rounded = (mid_fq / 100000) * 100000;

                        // Verify that the requested channels fit
                        if (lo_fq < (mid_fq_rounded - MAX_CH_OFFSET) || hi_fq > (mid_fq_rounded + MAX_CH_OFFSET)) {
                            std::cerr << "Error: Requested channels does not fit inside available IF bandwidth (" <<
                                         (MAX_CH_OFFSET*2)/1000000 << "MHz)" << std::endl;
                            ret = -1;
                        }

                        settings.tuner_fq = mid_fq_rounded;
                    }
                }
            } else {
                std::cerr << "Error: No channel given" << std::endl;
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


// Given a channel and tuner center frequency, return how many 8.33kHz steps
// the channel is with respect to the tuner center
static int channel_to_offset(const std::string &channel, int32_t tuner_fq) {
    int32_t fq_base;
    int32_t fq_diff;
    int32_t offset_diff;
    int     offset;

    // Dot is used as decimal separator
    auto dot_pos  = channel.find_first_of('.');
    auto int_str  = channel.substr(0, dot_pos); // integral part
    auto frac_str = channel.substr(dot_pos+1);  // fractional part

    // A 100kHz wide band "contains" 12 8.33kHz channels or 4 25kHz
    // channels. Each channel has it's unique two digits making it
    // possible to correctly convert a channel to offset in both schemas
    const std::map<std::string, int> sub_ch_map{
        { "00",  0 }, { "05",  0 }, { "10",  1 }, { "15",  2 },
        { "25",  3 }, { "30",  3 }, { "35",  4 }, { "40",  5 },
        { "50",  6 }, { "55",  6 }, { "60",  7 }, { "65",  8 },
        { "75",  9 }, { "80",  9 }, { "85", 10 }, { "90", 11 }
    };
    auto sub_offset = sub_ch_map.find(frac_str.substr(1));

    fq_base = std::atol(int_str.c_str())*1000000;
    fq_base += (frac_str[0] - '0') * 100000;
    fq_diff = fq_base - tuner_fq;
    offset_diff = (fq_diff / 100000)*12;
    offset = offset_diff + sub_offset->second;

    return offset;
}


int main(int argc, char** argv) {
    int              ret;
    struct sigaction sigact;
    uint32_t         sample_rate = RTL_IQ_SAMPLING_FQ;
    uint32_t         bw = 600000;
    Settings         settings;

    // Parse command line. Exit if incomplete or if help was requested
    if (parse_cmd_line(argc, argv, settings) < 0) {
        return 1;
    }

    // Setup the channels with "tuner", down sampler, AGC and audio position
    unsigned ch_idx = 0;
    for (auto &ch : settings.channels) {
        std::vector<iqsample_t> translator;

        int ch_offset = channel_to_offset(ch.name, (int32_t)settings.tuner_fq);
        if (ch_offset != 0) {
            int N = 144;   // This number depends on the IQ sampling fq
            for (int n = 0; n < N; n++) {
                std::complex<float> e(0.0f, -2.0f * M_PI * n * ch_offset/144.0f);
                translator.push_back(exp(e));
            }
        }

        ch.msd = MSD(translator, std::vector<MSD::Stage>{
            { 3, lp_ds_1200k_400k },
            { 5, lp_ds_400k_80k },
            { 5, lp_ds_80k_16k }
        });

        ch.agc.setReference(1.0f);
        ch.agc.setAttack(1.0f);
        ch.agc.setDecay(0.01f);

        ch.pos = get_audio_pos(ch_idx, settings.channels.size());
        ++ch_idx;
    }

    // Print settings
    std::cout << "The folowing settings were given:" << std::endl;
    std::cout << "    RTL device: " << settings.rtl_device << std::endl;
    std::cout << "    Frequency correction: " << settings.fq_corr << "ppm" << std::endl;
    std::cout << "    RF gain: " << settings.rf_gain << "dB" << std::endl;
    std::cout << "    LF (audio) gain: " << settings.lf_gain << "dB" << std::endl;
    std::cout << "    Squelch level: " << settings.sql_level << "dB" << std::endl;
    std::cout << "    ALSA device: " << settings.audio_device << std::endl;
    std::cout << "    Tuner frequency: " << settings.tuner_fq << "Hz" << std::endl;
    std::cout << "    Channels:";
    for (auto &ch : settings.channels) std::cout << " " << ch.name << "(" << ch.pos << ")";
    std::cout << std::endl;

    rb_t iq_rb(CH_IQ_BUF_SIZE * settings.channels.size(), 8); // 8 chunks or 256ms

    struct InputState input_state;
    input_state.settings   = settings;
    input_state.rtl_device = nullptr;
    input_state.rb_ptr     = &iq_rb;

    ret = rtlsdr_open(&input_state.rtl_device, settings.rtl_device);
    if (ret < 0 || input_state.rtl_device == NULL) {
        std::cerr << "Error: Unable to open device " << settings.rtl_device <<
                     ". ret = " << ret << std::endl;
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
    std::cout.precision(3);
    std::cout.width(3);
    for (int i = 0; i < num_tuner_gains; i++) {
        std::cout << " " << ((float)tuner_gains[i])/10.0f;
    }
    std::cout << std::endl;
    free(tuner_gains);
    tuner_gains = nullptr;

    std::cout << "Setting up device with ";
    std::cout << "Fq=" << settings.tuner_fq << "Hz, ";
    std::cout << "Corr=" << settings.fq_corr << "ppm, ";
    std::cout << "Bw=" << bw << "Hz, ";
    std::cout << "Gain=" << settings.rf_gain << "dB, ";
    std::cout << "Sample rate=" << sample_rate << " samples/s";
    std::cout << std::endl;

    ret = rtlsdr_set_center_freq(input_state.rtl_device, settings.tuner_fq);
    ret = rtlsdr_set_freq_correction(input_state.rtl_device, settings.fq_corr);
    ret = rtlsdr_set_tuner_bandwidth(input_state.rtl_device, bw);
    ret = rtlsdr_set_tuner_gain(input_state.rtl_device, settings.rf_gain * 10);
    ret = rtlsdr_set_sample_rate(input_state.rtl_device, sample_rate);

    // Check what happend
    uint32_t actual_sample_rate = rtlsdr_get_sample_rate(input_state.rtl_device);
    if (actual_sample_rate != sample_rate) {
        std::cerr << "Warning: Actual Sample rate: " << actual_sample_rate <<
                     "samples/s. Program will not work." << std::endl;
    }

    sigact.sa_handler = signal_handler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    sigaction(SIGQUIT, &sigact, NULL);
    sigaction(SIGPIPE, &sigact, NULL);

    struct OutputState output_state;
    output_state.rb_ptr   = &iq_rb;
    output_state.settings = settings;

    std::thread alsa_thread(alsa_worker, std::ref(output_state));
    std::thread dongle_thread(dongle_worker, std::ref(input_state));

    rtlsdr_reset_buffer(input_state.rtl_device);

    // RTL_IQ_BUF_SIZE is the size of the callback buffer. This will become len
    // in the data callback.
    ret = rtlsdr_read_async(input_state.rtl_device, iq_cb, &input_state, 4, RTL_IQ_BUF_SIZE);
    if (ret < 0) {
        std::cerr << "Error: rtlsdr_read_async returned: " << ret << std::endl;
        run = false;
    }

    dongle_thread.join();
    alsa_thread.join();

    std::cout << "Closing RTL device." << std::endl;
    rtlsdr_close(input_state.rtl_device);

    std::cout << "Stopped." << std::endl;
}
