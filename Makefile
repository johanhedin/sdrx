all: sdrx

rtl_dev.o: rtl_dev.hpp rtl_dev.cpp
	g++ -Wall -std=c++17 -g -O2 -I. -c rtl_dev.cpp

airspy_dev.o: airspy_dev.hpp airspy_dev.cpp
	g++ -Wall -std=c++17 -g -O2 -I. -Ilibairspy/libairspy/src -c airspy_dev.cpp

sdrx: coeffs.hpp iqsample.hpp msd.hpp crb.hpp fir.hpp agc.hpp sdrx.cpp
	g++ -Wall -std=c++17 -O2 -I. -o sdrx sdrx.cpp -lpopt -lpthread -lusb-1.0 -lrtlsdr -lasound -lm -lfftw3f

dts: dts.cpp rtl_dev.o airspy_dev.o
	g++ -Wall -std=c++17 -g -O2 -I. -o dts dts.cpp rtl_dev.o airspy_dev.o build/libairspy/libairspy/src/libairspy.a -lpthread -lusb-1.0 -lrtlsdr

clean:
	rm -f *.o sdrx dts

