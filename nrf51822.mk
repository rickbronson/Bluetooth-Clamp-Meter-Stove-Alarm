export GCC_BASE=/opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3
export GCC_CROSS=arm-none-eabi

export GCC_VERSION=4.7.4
export PATH := ":$(GCC_BASE)/bin:$(OPT_HOME)/scripts:$(PATH)"
export CROSS_COMPILE=$(GCC_CROSS)-
export CC=$(CROSS_COMPILE)gcc
export CXX=$(CROSS_COMPILE)g++
export LD=$(CROSS_COMPILE)ld
export AR=$(CROSS_COMPILE)ar
export OPT_SYSROOT=$(GCC_BASE)/$(GCC_CROSS)/libc
export OPT_LINUX=$(OPT_HOME)/linux
export KERNEL_DIR=$(OPT_LINUX)
export OPT_INC=$(OPT_LINUX)/include
export OPT_USR_INC=$(OPT_HOME)/rd/usr/include
export BUILDROOT=buildroot-2010.11
export LIBDIRS=-L $(OPT_HOME)/rd/usr/lib/arm-linux-gnueabihf -L $(OPT_HOME)/rd/lib/arm-linux-gnueabihf -L $(GCC_BASE)/lib -L $(GCC_BASE)/$(GCC_CROSS)/lib -L $(GCC_BASE)/lib/gcc/$(GCC_CROSS)/$(GCC_VERSION) -L $(GCC_BASE)/$(GCC_CROSS)/libc/usr/lib
export IINC=-isystem $(OPT_USR_INC) \
	-isystem $(GCC_BASE)/$(GCC_CROSS)/include/c++/$(GCC_VERSION) \
	-isystem $(GCC_BASE)/$(GCC_CROSS)/include/c++/$(GCC_VERSION)/$(GCC_CROSS) \
	-isystem $(GCC_BASE)/$(GCC_CROSS)/include/c++/$(GCC_VERSION)/backward \
	-isystem $(GCC_BASE)/lib/gcc/$(GCC_CROSS)/$(GCC_VERSION)/include \
	-isystem $(GCC_BASE)/lib/gcc/$(GCC_CROSS)/$(GCC_VERSION)/include-fixed
OBJCOPY=$(CROSS_COMPILE)objcopy
OBJDUMP=$(CROSS_COMPILE)objdump
RAM_LOAD=0x80008000  # must be somewhere in RAM on target