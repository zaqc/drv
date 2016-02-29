KDIR="~/work/DE0Linux/linux-socfpga (copy)"
ARMMAKE=make ARCH=arm SUBARCH=arm CROSS_COMPILE=arm-linux-gnueabi-

obj-m := mydrv.o

mydrv.ko: mydrv.c
	$(ARMMAKE) -C $(KDIR) M=$(PWD) modules

clean:
	rm -f *.ko *.o *.mod.c *.symvers *.order

