ARCH?=$(shell uname -m)
CROSS_COMPILE?=aarch64-linux-gnu-

all: build

build:
ifeq ($(ARCH),aarch64)
	gcc car_server.c -o car_server
else
	$(CROSS_COMPILE)gcc car_server.c -o car_server
endif

clean:
	rm -f car_server