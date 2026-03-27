all: static/fit_zip dynamic/fit_zip dynamic/fit_unzip

heatshrink:
	git clone https://github.com/atomicobject/heatshrink

static/fit_zip: heatshrink fit_delta_encode.c
	cd static && make fit_zip

dynamic/fit_zip: heatshrink fit_delta_encode.c
	cd dynamic && make fit_zip

dynamic/fit_unzip: heatshrink fit_delta_encode.c
	cd dynamic && make fit_unzip

clean:
	cd static && make clean
	cd dynamic && make clean

