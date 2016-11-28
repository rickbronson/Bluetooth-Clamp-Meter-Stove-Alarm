PROJ=nrf51822
export OPT_HOME := $(shell pwd)
include $(OPT_HOME)/${PROJ}.mk
# set up for parallel make's if make is new enough
export MK_PAR=$(shell if make -v | grep "3.80" > /dev/null ; then echo -n; else echo -n "-j9"; fi)
TMPMNT=/tmp/mnt
$(shell mkdir -p $(TMPMNT))
#APPDIRNAME=ble_app_proximity
#APPNAME=ble_app_proximity
APPDIRNAME=adc-example-with-softdevice
APPNAME=ble_app_hrs
APPDIR=nrf51_sdk_v6_0_0_43681/nrf51822/Board/pca10001/s110/$(APPDIRNAME)/gcc
APP=$(APPDIR)/_build/$(APPNAME)_s110_xxaa
VERS=1
OUT_DIR= $(shell	if [ -d "$(HOME)/in" ] ; then \
		echo -n $(HOME)/in; else echo -n /tmp;	fi)

.PHONY: kernel xloader buildroot

all:
	make clean
	make sdk

lis:
	cd nrf51_sdk_v6_0_0_43681/nrf51822/Board/pca10001/s110/adc-example-with-softdevice/gcc; "/opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3/bin/arm-none-eabi-gcc" -Wa,-alh,-L -DDEBUG_NRF_USER -DBLE_STACK_SUPPORT_REQD -DS110 -ffunction-sections -mcpu=cortex-m0 -mthumb -mabi=aapcs -DNRF51 -DBOARD_NRF6310 -DNRF51822_QFAA_CA --std=gnu99 -Wall -Werror -mfloat-abi=soft -DDEBUG -g3 -O0 -I"../../../../../Include/s110" -I"../../../../../Include/ble" -I"../../../../../Include/ble/device_manager" -I"../../../../../Include/ble/ble_services" -I"../../../../../Include/app_common" -I"../../../../../Include/sd_common" -I"../../../../../Include/sdk" -I"../" -I"../../../../../Include" -I"../../../../../Include/gcc" -I"../../../../../Include/ext_sensors" -c -o _build/main.o ../main.c > main.lis

clean:
	cd $(APPDIR); make -f $(APPNAME).Makefile clean

flash:
	openocd -f interface/stlink-v2.cfg -f target/nrf51.cfg -c init -c "mww 0x4001e504 1" -c "program $(APP).bin 0x16000" -c shutdown

split:
	$(OBJCOPY) -I ihex -O binary s110/s110_nrf51822_7.0.0_softdevice.hex s110.bin

erase:
	openocd -f interface/stlink-v2.cfg -f target/nrf51.cfg -c init -c "mww 0x4001e504 2" -c "mww 0x4001e50c 1" -c "mww 0x4001e514 1" -c shutdown

sdk:
	cd $(APPDIR); make -f $(APPNAME).Makefile

dis:
	$(OBJDUMP) -S $(APP).out > dis.lis


gdb:
	/opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3/bin/arm-none-eabi-gdb -x ./nrf51_sdk_v6_0_0_43681/nrf51822/Board/pca10001/s110/adc-example-with-softdevice/gcc/_build/ble_app_hrs_s110_xxaa.out

gordon:
#	cd ../../; tar -c -j --exclude-vcs --exclude "boards/${PROJ}/code" --exclude '*~' --exclude '*.lst' --exclude '*.lis' --exclude '.dep*' --exclude '*.dep' opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3 boards/nrf51822 --file=$(OUT_DIR)/$@-$(PROJ)-$(VERS).tar.bz2
	(  echo "open ftp.efn.org"; echo "user rick `cat $(HOME)/scripts/ppp/passwrd.efn`"; echo "cd public_html/pub/${PROJ}"; echo "lcd $(OUT_DIR)"; echo "put $@-$(PROJ)-$(VERS).tar.bz2";  echo "bye";  echo "quit";  ) | ftp -n -i -v 2>&1 >> $(OUT_DIR)/ftp.log
	@echo -e "sudo mv -f /opt /opt.old"
	@echo -e "sudo ln -s \044HOME/opt /opt"
	@echo -e "wget -P /tmp http://efn.org/~rick/pub/$(PROJ)/$@-$(PROJ)-$(VERS).tar.bz2"
	@echo -e "tar xjf /tmp/$@-$(PROJ)-$(VERS).tar.bz2 -C \044HOME"
	@echo -e "ln -s ~/boards/nrf51822 ~/opt/nrf51822"
	@echo -e "sudo apt-get install git libtool automake"
	@echo -e "cd ~/boards/nrf51822"
	@echo -e "git clone git://git.code.sf.net/p/openocd/code"
	@echo -e "cd code"
	@echo -e "./bootstrap"
	@echo -e "./configure"
	@echo -e "make"
	@echo -e "sudo make install"
	@echo -e "cd ~/boards/nrf51822"
	@echo -e "make"
