ARCH?=$(shell uname -m)
CROSS_COMPILE?=aarch64-linux-gnu-

all: build

build:
ifeq ($(ARCH),aarch64)
	gcc goal_server.c -o goal_server
else
	$(CROSS_COMPILE)gcc goal_server.c -o goal_server -lwiringPi
endif

clean:
	rm -f goal_server