all: sdrx

rtl_device.o: rtl_device.hpp rtl_device.cpp
	g++ -c -g -Wall -std=c++17 -I. rtl_device.cpp

sdrx: coeffs.hpp iqsample.hpp msd.hpp rb.hpp fir.hpp agc.hpp sdrx.cpp rtl_device.o
	g++ -o sdrx -O2 -Wall -std=c++17 -I. sdrx.cpp rtl_device.o -lpopt -lpthread -lusb-1.0 -lrtlsdr -lasound -lm -lfftw3f

dts: dts.cpp rtl_device.o
	g++ -o dts -g -Wall -std=c++17 -I. dts.cpp rtl_device.o -lrtlsdr -lpthread

clean:
	rm -f *.o
	rm -f sdrx dts
