//
// Test for future RTL and Airspy device classes
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

// Standard C includes
#include <string.h>
#include <signal.h>

// Standard C++ includes
#include <thread>
#include <chrono>
#include <iostream>

// Libraries we use
#include <popt.h>

// Local includes
#include "rtl_dev.hpp"
#include "airspy_dev.hpp"

static std::string serial;
static bool        run = true;
static unsigned    callback_counter = 0;
static unsigned    sample_counter = 0;
std::chrono::time_point<std::chrono::system_clock> ts1;
std::chrono::time_point<std::chrono::system_clock> ts2;


static void signal_handler(int signo) {
    std::cout << "Signal '" << strsignal(signo) << "' received. Stopping...\n";
    run = false;
}


static void on_data(const iqsample_t *, unsigned data_len, void*, const BlockInfo&) {
    if (callback_counter == 0) {
        ts1 = std::chrono::system_clock::now();
    } else {
        sample_counter += data_len;
    }

    if (++callback_counter == 30) {
        ts2 = std::chrono::system_clock::now();

        unsigned elaped_time_us = std::chrono::duration_cast<std::chrono::microseconds>(ts2 - ts1).count();
        double samples_per_second = (double)sample_counter / (double)elaped_time_us;
        double callbacks_per_second = ((double)(callback_counter-1) / (double)elaped_time_us)*1000000;

        std::cout << "on_data: device = " << serial << ", data_len = " <<  data_len <<
                    ", rate = " << samples_per_second << " MS/s / " << callbacks_per_second << " callbacks/s" << std::endl;
        callback_counter = 0;
        sample_counter = 0;
    }
}


int main(int argc, char **argv) {
    int               ret;
    int               print_help = 0;
    int               list_devices = 0;
    int               run_test = 0;
    char             *tmp_serial = nullptr;
    char             *tmp_rate = nullptr;
    SampleRate        fs = SampleRate::UNSPECIFIED;
    poptContext       popt_ctx;

    // Fill in the options config
    struct poptOption options_table[] = {
        { "list",      'l', POPT_ARG_NONE,   &list_devices, 0, "list available devices", nullptr },
        { "rate",      'r', POPT_ARG_STRING, &tmp_rate,     0, "use this sample rate", "SAMPLE_RATE" },
        { "test",      't', POPT_ARG_STRING, &tmp_serial,   0, "run test with given device", "SERIAL" },
        { "help",      'h', POPT_ARG_NONE,   &print_help,   0, "show full help and quit", nullptr },
        POPT_TABLEEND
    };

    // Create popt instance
    popt_ctx = poptGetContext(nullptr, argc, (const char**)argv, options_table, POPT_CONTEXT_POSIXMEHARDER);
    poptSetOtherOptionHelp(popt_ctx, "[-h, --help] [-l, --list] [-r, --rate SAMPLE_RATE] [-t, --test SERIAL]");

    while ((ret = poptGetNextOpt(popt_ctx)) > 0);
    if (ret < -1) {
        // Error while parsing the options. Print the reason
        switch (ret) {
            case POPT_ERROR_BADOPT:
                std::cerr << "Error: Unknown option given.\n";
                break;
            case POPT_ERROR_NOARG:
                std::cerr << "Error: Missing option value.\n";
                break;
            case POPT_ERROR_BADNUMBER:
            case POPT_ERROR_OVERFLOW:
                std::cerr << "Error: Option could not be converted to number.\n";
                break;
            default:
                std::cerr << "Error: Unknown error in option parsing, ret = " << ret << ".\n";
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
Write some more here...)"
            << std::endl;
            ret = -1;
        } else {
            // All good. Settings inside variables
            if (tmp_rate) {
                fs = str_to_sample_rate(std::string(tmp_rate));
                free(tmp_rate);
            }

            if (tmp_serial) {
                run_test = 1;
                serial = tmp_serial;
                free(tmp_serial);
            }

            if (!list_devices && !run_test) {
                poptPrintHelp(popt_ctx, stderr, 0);
            }
        }
    }

    poptFreeContext(popt_ctx);

    // List devices and then exit
    if (list_devices) {
        std::cout << "Available RTL devices..." << std::endl;
        std::vector<RtlDev::Info> rtl_devices = RtlDev::list();
        for (auto &dev : rtl_devices) {
            std::cout << "    " << dev.index  << ", " << dev.serial;
            if (dev.available) {
                if (dev.supported) {
                    std::cout << ", sample rates =";
                    bool first = true;
                    for (auto &rate : dev.sample_rates) {
                        if (first) {
                            std::cout << " " << sample_rate_to_str(rate) << "MS/s";
                            first = false;
                        } else {
                            std::cout << ", " << sample_rate_to_str(rate) << "MS/s";
                        }
                    }

                    std::cout << ", description = " << dev.description << std::endl;
                } else {
                    std::cout << " (unsupported tuner and/or crystal fq)\n";
                }
            } else {
                std::cout << " (in use)\n";
            }
        }

        std::cout << "Available Airspy devices...\n";
        std::vector<AirspyDev::Info> airspy_devices = AirspyDev::list();
        for (auto &dev : airspy_devices) {
            std::cout << "    " << dev.index  << ", " << dev.serial;
            if (dev.available) {
                if (dev.supported) {
                    std::cout << ", sample rates =";
                    bool first = true;
                    for (auto &rate : dev.sample_rates) {
                        if (first) {
                            std::cout << " " << sample_rate_to_str(rate) << "MS/s";
                            first = false;
                        } else {
                            std::cout << ", " << sample_rate_to_str(rate) << "MS/s";
                        }
                    }

                    std::cout << ", description = " << dev.description << std::endl;
                } else {
                    std::cout << " (unsupported model and/or sample rate)\n";
                }
            } else {
                std::cout << " (in use)\n";
            }
        }

        return 0;
    }

    if (run_test) {
        // Install handler for Crtl-C
        struct sigaction sigact;
        sigact.sa_handler = signal_handler;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigaction(SIGINT, &sigact, NULL);
        sigaction(SIGTERM, &sigact, NULL);
        sigaction(SIGQUIT, &sigact, NULL);
        sigaction(SIGPIPE, &sigact, NULL);

        if (RtlDev::isPresent(serial)) {
            std::cout << "Running test with RTL device " << serial << " @ " << sample_rate_to_str(fs) << "MS/s. Press Ctrl-C to stop\n";

            RtlDev *dev = new RtlDev(serial, fs);

            dev->data.connect(sigc::ptr_fun(on_data));
            dev->setGain();
            ret = dev->start();
            if (ret < 0) {
                std::cerr << "Error: Unable to start device, ret = " << ret << " (" << RtlDev::errStr(ret) << ")\n";
            } else {
                while (run) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                dev->stop();
            }

            delete dev;
        } else if (AirspyDev::isPresent(serial)) {
            std::cout << "Running test with Airspy device " << serial << " @ " << sample_rate_to_str(fs) << "MS/s. Press Ctrl-C to stop\n";

            AirspyDev *dev = new AirspyDev(serial, fs);

            dev->data.connect(sigc::ptr_fun(on_data));
            ret = dev->start();
            if (ret < 0) {
                std::cerr << "Error: Unable to start device, ret = " << ret << " (" << AirspyDev::errStr(ret) << ")\n";
            } else {
                while (run) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                }
                dev->stop();
            }

            delete dev;
        } else {
            std::cerr << "Error: No device with serial " << serial << " found.\n";
        }

        return 0;
    }

    return 0;
}
