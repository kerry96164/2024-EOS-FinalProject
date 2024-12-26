obj-m := car_driver.o
linux_src := /home/kerry/EOS-2024/linux

all: RPI_build
clean: RPI_clean

RPI_build: car_driver.c
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(linux_src) M=$(PWD) modules
 
RPI_clean:
	sudo make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -C $(linux_src) M=$(PWD) clean

 
.PHONY: 
	all clean RPI_build RPI_clean