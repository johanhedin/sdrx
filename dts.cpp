//
// Test for future RTL and Airspy device classes
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

#include <thread>
#include <chrono>
#include <iostream>

#include <string.h>
#include <signal.h>

#include "rtl_dev.hpp"
#include "airspy_dev.hpp"

static bool run = true;


static void signal_handler(int signo) {
    std::cout << "Signal '" << strsignal(signo) << "' received. Stopping...\n";
    run = false;
}


int main(int argc, char **argv) {
    struct sigaction sigact;
    bool             list_devices = false;
    bool             run_test = false;
    std::string      serial;

    // Collect command line arguments
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--list" || arg == "-l") {
            list_devices = true;
        } else if ((arg == "--test" || arg == "-t") && argc > 2) {
            serial = argv[2];
            run_test = true;
        }
    }

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
                            std::cout << " " << rate;
                            first = false;
                        } else {
                            std::cout << ", " << rate;
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

        std::cout << "Available Airspy devices..." << std::endl;
        std::vector<AirspyDev::Info> airspy_devices = AirspyDev::list();
        for (auto &dev : airspy_devices) {
            std::cout << "    " << dev.index  << ", " << dev.serial;
            if (dev.available) {
                if (dev.supported) {
                    std::cout << ", sample rates =";
                    bool first = true;
                    for (auto &rate : dev.sample_rates) {
                        if (first) {
                            std::cout << " " << rate;
                            first = false;
                        } else {
                            std::cout << ", " << rate;
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
        sigact.sa_handler = signal_handler;
        sigemptyset(&sigact.sa_mask);
        sigact.sa_flags = 0;
        sigaction(SIGINT, &sigact, NULL);
        sigaction(SIGTERM, &sigact, NULL);
        sigaction(SIGQUIT, &sigact, NULL);
        sigaction(SIGPIPE, &sigact, NULL);

        if (RtlDev::present(serial)) {
            std::cout << "Running test with RTL device " << serial << std::endl;

            RtlDev *dev = new RtlDev(serial);

            dev->start();
            while (run) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            dev->stop();

            delete dev;
        } else if (AirspyDev::present(serial)) {
            std::cout << "Running test with Airspy device " << serial << std::endl;
            uint32_t fs = 3000000;

            AirspyDev *dev = new AirspyDev(serial, fs);

            dev->start();
            while (run) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            }
            dev->stop();

            delete dev;
        } else {
            std::cerr << "Error: No device with serial " << serial << " found.\n";
        }

        return 0;
    }

    std::cout << "Usage: dts [--list,-l] [--test,-t <serial>]\n";

    return 0;
}
