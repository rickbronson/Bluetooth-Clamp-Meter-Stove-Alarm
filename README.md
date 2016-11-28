  MT87 Clamp Meter Stove Alarm
==========================================

1. The Problem
--------------

  This project started as a way to warn autistic occupants of a home
that they left the stove on for an extended period of time.  Several
solutions were explored.

..* A new stove with a "left on" alarm.  Some stoves have this feature
	but a new stove was beyond the financial reach of this project.
..* Stove alarm phone app's but these require you to enter when you start
	the stove.  Not a solution, really.
..* After-market stove alarms.  These were too expensive also.
..* Clamp meter (about US$10) that measures appliance current draw without invasive
	wiring.  Note that the clamp part of the clamp meter only works when
	it clamps just one of the wires of the two or three wires that make up
	a power cord.  This usually means that you are required to split the
	cable so that you can clamp around just one wire. The idea of
	marrying a Bluetooth Low Energy (BLE) module (about US$10) and
	a piezo buzzer with the clamp meter was an afterthought.

2. Ideas
--------------

  The clamp meter was bought [Clamp Meter](http://www.aliexpress.com/item/Free-Drop-shipping-MT87-Digital-LCD-Display-Clamp-meter-Multimeter-Ohm-DMM-DC-AC-Current-Voltmeter/1874051779.html

  The Bluetooth module was bought [Bluetooth](http://www.aliexpress.com/item/NRF51822-2-4G-Wireless-Module-Wireless-Communication-Module-Bluetooth-module-zigbee-module-DMX512/1928935008.html

  I tried to figure out what chip the MT87 uses but was unsuccessful.
  It's probably close to a Intersil ICL7107 or clone.  So some reverse
  engineering was in order.  It was determined that the analog input
  to the chip was at D in the picture below and that the switched 3
  volts from the MT87 battery was at the bottom of R45 at A in the
  picture below.  Bring B low to turn on the piezo buzzer.  C is the
  input for volts and resistance measurement.  D is for current
  measurement.  E is +3 volts and F is GND.  The boards are wired as such:

| MT87          | nrf81522      |
| ------------- | ------------- |
|A              |   VDD         |
|B              |   P0.18       |
|C              |  N/A          |
|D              |   P0.01       |
|E              |   N/A         |
|F              |   GND         |

![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/mt87-6.png)

  As far as programming, I used a ST Discovery board's SWD programming
  port and hooked it up like this.

![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/yunjia-wiring.jpg)
NOTE: picture gotten from [here](https://github.com/RIOT-OS/RIOT/wiki/Board:-yunjia-nrf51822)

  Here is the whole mess hooked up with the progammer
![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/mt87-3.jpg)

  Here is the end product with the board hot-glued in and ready to
  screw together.  I added in a jack for power with a 3.3 volt
  regulator soldered in at the jack.  I had to clip off the header
  pins from the bottom of the bluetooth board to get it down in height
  so it would fit inside the meter.

![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/mt87-5.jpg)

3. Programming
--------------

  Note that the alarm will work without using Bluetooth.  On a
  modified Nordic Android App (nRFTemp) the wattage shows up under
  temperature and the temperature shows up in the battery icon.  See
  pictures below.

![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/mt87-7.png)
![alt text](https://github.com/rickbronson/Bluetooth-Clamp-Meter-Stove-Alarm/blob/master/images/mt87-8.png)

4. Set up
---------

  Here are some notes on how to set it all up on Linux:

1. Get Key from Nordic Semiconductor so that you can download:
a. nrf51_sdk_v6_0_0_43681.zip
b. s110_nrf51822_7.0.0.zip

NOTE: If you want to get an older version of s110 (the bluetooth part)
	try here:
https://github.com/finnurtorfa/nrf51

```
mkdir -p ~/boards/nrf51822
unzip nrf51_sdk_v6_0_0_43681.zip -d ~/boards/nrf51822/s110
unzip s110_nrf51822_7.0.0.zip -d ~/boards/nrf51822/nrf51_sdk_v6_0_0_43681
cd ~/boards/nrf51822
git clone https://github.com/NordicSemiconductor/nrf51-ADC-examples
mv nrf51-ADC-examples/adc-example-with-softdevice nrf51_sdk_v6_0_0_43681/nrf51822/Board/pca10001/s110

# Get my Makefile and nrf51822.mk (above) and put in ~/boards/nrf51822, get main.c and
	put in ~/boards/nrf51822/nrf51_sdk_v6_0_0_43681/nrf51822/Board/pca10001/s110/adc-example-with-softdevice/main.c

# you might have to be root for these, on my box I have a link from
  /opt to my home dir:
lrwxrwxrwx   1 root root    14 Dec 14  2012 opt -> /home/rick/opt

mkdir -p /opt/CodeSourcery
wget https://launchpad.net/gcc-arm-embedded/4.7/4.7-2013-q3-update/+download/gcc-arm-none-eabi-4_7-2013q3-20130916-linux.tar.bz2 -P /opt/CodeSourcery
tar xzf /opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3-20130916-linux.tar.bz2 -C /opt/CodeSourcery
```

  I used openocd to program the part but had to get the very latest
  git:

```
sudo apt-get install git libtool automake
git clone git://git.code.sf.net/p/openocd/code
cd code
# read the INSTALL file or just do:
./bootstrap
./configure
make
sudo make install
```
Do one mod:

```
diff -b -c /home/rick/boards/nrf51822/nrf51_sdk_v6_0_0_43681/nrf51822/Source/templates/gcc/Makefile.posix.\~1\~ /home/rick/boards/nrf51822/nrf51_sdk_v6_0_0_43681/nrf51822/Source/templates/gcc/Makefile.posix
*** /home/rick/boards/nrf51822/nrf51_sdk_v6_0_0_43681/nrf51822/Source/templates/gcc/Makefile.posix.~1~	2014-07-01 15:31:40.000000000 -0700
--- /home/rick/boards/nrf51822/nrf51_sdk_v6_0_0_43681/nrf51822/Source/templates/gcc/Makefile.posix	2014-08-11 13:31:35.000000000 -0700
***************
*** 1,4 ****
! GNU_INSTALL_ROOT := /usr/local/gcc-arm-none-eabi-4_8-2014q1
! GNU_VERSION := 4.8.3
  GNU_PREFIX := arm-none-eabi
  
--- 1,4 ----
! GNU_INSTALL_ROOT := /opt/CodeSourcery/gcc-arm-none-eabi-4_7-2013q3
! GNU_VERSION := 4.7.4
  GNU_PREFIX := arm-none-eabi
```

  Then do:

```
cd ~/boards/nrf51822
make
```

  It should build.
