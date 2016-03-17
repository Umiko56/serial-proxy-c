# Serial Proxy

`serial-proxy` is a Linux daemon that will proxy data from any number of
serial devices (RS232) to their respective pseudo-terminals (virtual serial
ports). This allows more than one application to receive raw data from a single
serial port and pretend it is reading from a physical device.

    /dev/ttyS1 (Garmin GPS)  /dev/ttyUSB1 (Temperature readings)
         |                         |
        / \                       /|\
       /   \                     / | \
      /     \                   /  |  \
    App1   App2              App3 App4 App5

This is a C port of [rkubik/serial-proxy](https://github.com/rkubik/serial-proxy).

## Features

- C
- Less than 100 KB on disk
- Dependency free
- Simple INI configuration
- Linux only (pull requests welcome for other platforms)

## Installation

    git clone ...
    cd serial-proxy-c
    mkdir build
    cd build

    cmake ..
    make
    make install

    debuild -us -uc -b
    dpkg -i ../serial-proxy_*.deb

Debugging:

    dpkg -i ../serial-proxy-dbg_*.deb

## Usage

serial-proxy is driven by two INI config files:

1. sproxy.ini - system configuration (optional). Can be set via `-c` command-line
   argument. The systemd service file will set `-c` to /etc/serial-proxy/sproxy.ini`.

Example:

    [logging]
    loglevel = debug
    syslog-enabled = no
    logfile = /var/log/sproxy.log

    [system]
    pidfile = /var/run/sproxyd.pid
    serial-configfile = /etc/serial-proxy/serial.ini
    hz = 10

2. serial.ini - serial port configuration. Set via system configuration file
   using `serial-configfile`. Default: `serial.ini`. The serial port device

Example:

    [/dev/ttyS5]
    baudrate = 38400
    virtuals = a b c

    [/dev/ttyS6]
    baudrate = 9600
    virtuals = d e f

## TODO

- Allow virtuals writers
- Unit testing
- More platform support
