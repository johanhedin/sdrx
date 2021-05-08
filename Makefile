all: sdrx

rtl_device.o: rtl_device.hpp rtl_device.cpp
	g++ -Wall -std=c++17 -O2 -I. -c rtl_device.cpp

airspy_dev.o: airspy_dev.hpp airspy_dev.cpp
	g++ -Wall -std=c++17 -O2 -I. -c airspy_dev.cpp

sdrx: coeffs.hpp iqsample.hpp msd.hpp crb.hpp fir.hpp agc.hpp sdrx.cpp
	g++ -Wall -std=c++17 -O2 -I. -o sdrx sdrx.cpp -lpopt -lpthread -lusb-1.0 -lrtlsdr -lasound -lm -lfftw3f

dts: dts.cpp rtl_device.o airspy_dev.o
	g++ -Wall -std=c++17 -O2 -I. -o dts dts.cpp rtl_device.o airspy_dev.o -lpthread -lusb-1.0 -lrtlsdr -lairspy

clean:
	rm -f *.o sdrx dts

