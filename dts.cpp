//
// Test for future RTL Device class
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

#include <thread>
#include <chrono>
#include <iostream>

#include <stdio.h>

#include "rtl_device.hpp"

int main(int argc, char **argv) {
    RtlDevice d0(0), d1(1);
    unsigned i = 0;
    char buf[128];

    snprintf(buf,128, argv[1]);

    std::cout << "Staring dongles..." << std::endl;

    d0.start();
    d1.start();

    while (i < 5) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        i++;
    }

    d1.stop();
    d0.stop();

    std::cout << "All dongles stoped." << std::endl;

    return 0;
}
