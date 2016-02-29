# drv
before make clone linux-socfpga into "~/work/DE0Linux/linux-socfpga (copy)" directory
or change path to it in Makefile ($KDIR)

git clone https://github.com/altera-opensource/linux-socfpga.git

make ARCH=arm socfpga_defconfig
