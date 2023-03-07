<a id="Top"></a>

# DHT22 driver for Raspberry Pi OS

[![Contributors](https://img.shields.io/github/contributors/hofjak/dht22-sensor)](https://github.com/hofjak/dht22-sensor)
[![Forks](https://img.shields.io/github/forks/hofjak/dht22-sensor)](https://github.com/hofjak/dht22-sensor)
[![Stars](https://img.shields.io/github/stars/hofjak/dht22-sensor)](https://github.com/hofjak/dht22-sensor)
[![Issues](https://img.shields.io/github/issues/hofjak/dht22-sensor)](https://github.com/hofjak/dht22-sensor)
[![Licence](https://img.shields.io/github/license/hofjak/dht22-sensor)](https://github.com/hofjak/dht22-sensor)



This is a Raspberry Pi kernel module for the DHT22 humidity and temperature sensor, 
implemented as a character device driver. The aim of this project was to learn how to write, 
compile and cross-compile a kernel module for Linux and to get familiar with CMake. 
It is not by any means supposed to be a production-grade solution.



## Table of contents
* [Description](#Description)
* [Build](#Build)
* [Installation](#Installation)
* [Example program](#Example-program)
* [Implementation](#Implementation)
* [Credits and inspiration](#Credits-and-Inspiration)
* [License](#License)



## Description

This module adds the device file `/dev/dht22` to the system and to keep it simple, it can only be opened by one
process at a time. To retrieve the basic sensor output run `cat`.

```bash 
cat /dev/dht22
```

A successful read returns a character string with two comma-separated decimal numbers e.g.
`"45.6,23.4"`. The first value represents the relative humidity as a percentage and the second value represents 
the temperature in degrees Celsius. There will be at most 13 ASCII characters 
(8 digits, 2 decimal points, 1 comma and 1 minus sign) inside the returned string. Note that
the terminating null-byte is not included.
If the read fails or another error occurs, `errno` is set, and the return value is `-1`. The different error codes
are described [here](#Error-codes).

By default, GPIO-pin 4 is used for data transmission. 
If you want to use a different pin you can specify the `gpio_pin` module parameter when inserting the module
into the kernel.



## Build

To build the module first clone the repository and navigate into the 
`dht22-sensor` directory.

```bash
git clone https://github.com/hofjak/dht22-sensor.git && cd dht22-sensor
```

For demonstration purposes I use the `build` folder inside the project source directory to build the module. 
But I strongly recommend using a TMPFS, especially to build larger projects.

```bash
mkdir build
```

### Native

If you are building natively on the Raspberry Pi for your current running kernel, make sure you have the kernel 
headers installed. You can verify the installation by running:

```bash
ls /usr/src | grep linux-headers
```

If there is some output they are already installed. If not, run:

```bash
sudo apt install raspberrypi-kernel-headers
```

After the environment is set up, you can copy-paste the following command to configure CMake and 
build the module:

```bash
cmake -B build && make -C build build-module
```

The compiled module will be located under `build/src/dht22.ko`.
To clean up the `build` folder run:

```bash
make -C build clean-module
```

### Cross-compile

If, on the other hand, you want to cross-compile you can use a
CMake toolchain file like `cmake/Toolchain.cmake` and specify the necessary parameters. 
Comment out the parts that you don't need. 
You have to specify the toolchain file like this when configuring CMake:


```bash
cmake -B build --toolchain=./cmake/Toolchain.cmake
```

You can also specify the needed variables via the command line.
To see what variables are available, have a look at `cmake/Toolchain.cmake`.



## Installation

To load the module into the kernel run:

```bash
sudo insmod build/src/dht22.ko
```
Once the module has been loaded, the device `/dev/dht22` should have been created. 
You can also verify that the module has been loaded by running:

```bash
lsmod | grep dht22
```
To unload the module from the kernel, run:

```bash
sudo rmmod dht22
```

By default, the created device can only be accessed with root privileges.
To make the device accessible to regular users, add proper udev-rules to `/etc/udev/rules.d/` or 
copy the provided `42-dht22.rules` file.

```bash
sudo cp 42-dht22.rules /etc/udev/rules.d
```


## Example program

For a more exhaustive usage demonstration have a look at the provided example program `examples/example.c`.
To build it, configure CMake and run:

```bash
make -C build build-examples
```


## Implementation

Initially, the GPIO pin for the data transmission is pulled high. After the two-pulse handshake sequence to 
trigger a read (1 host and 1 slave) the sensor generates 40 pulses which encode the data. 
Every new bit starts with a 50µs low pulse and a following high pulse. 
The length of the high pulse determines whether the transferred bit is a 0 or a 1. 
According to the datasheet a length of around 26µs to 28µs encodes a 0 and a length of 70µs encodes a 1.

I use an interrupt handler that triggers on every falling edge to measure the pulse width.
Thus, only the time between the 50µs low pulses can be measured. I found 110µs to be a good 
decision boundary between reading a 0 or a 1. If you want to look into the timing yourself you can build 
the module using the CMake debug configuration. Measured pulse widths and other debug information will be 
printed to the kernel message buffer. You can read it running `dmesg`.

Check out the full dht22 datasheet 
[here](https://cdn-shop.adafruit.com/datasheets/Digital+humidity+and+temperature+sensor+AM2302.pdf).

<center> <a id="Error-codes"></a> 

| Error codes | Reason                                      |
| :---------- | :------------------------------------------ |
| `EBUSY`     | Someone else is already using the device    |
| `EIO`       | Triggering sensor failed - GPIO error       |
| `ERANGE`    | Checksum invalid - Faulty sensor read       |

</center>



## Credits and inspiration
While I was looking for a similar project to collect some ideas, I came across 
[this](https://github.com/krepost/dht22) demo module by [@krepost](https://github.com/krepost). 
It really helped me feel more comfortable with the subject.


Furthermore, I want to share some other resources I found to be very useful:
* [Driver development on a Raspberry Pi](https://kokkonisd.github.io/2020/10/24/linux-drivers-rpi)
* [Kernel building](https://suhu0426.github.io/Web/Howto-Install/Xvisor/KernelBuildingForRaspberryPi.html)
* [Raspberry Pi toolchains](https://github.com/abhiTronix/raspberry-pi-cross-compilers)



## License
 
[GPL-3.0](./LICENSE)



⬆️ [Back to top](#Top)

---
> GitHub [@hofjak](https://github.com/hofjak)  &nbsp;&middot;&nbsp;
> Email jakob.refoh@gmail.com
