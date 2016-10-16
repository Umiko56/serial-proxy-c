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
- Virtual writers
- Event-driven, single process

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

1. sproxy.ini - system configuration (optional). Can be set via `-c`
   command-line argument. The systemd service file will set `-c` to 
   `/etc/serial-proxy/sproxy.ini`.

Example:

    [logging]
    loglevel = debug
    syslog-enabled = no
    logfile = /var/log/sproxy.log

    [system]
    pidfile = /var/run/sproxyd.pid
    serial-configfile = /etc/serial-proxy/serial.ini
    hz = 10
    reconnect-interval = 5000

2. serial.ini - serial port configuration. Set via system configuration file
   using `serial-configfile`. Default: `serial.ini`.

Example:

    [/dev/ttyS5]
    baudrate = 38400
    virtuals = a b c
    writer = a

    [/dev/ttyS6]
    baudrate = 9600
    virtuals = d e f

The serial configuration file above will attempt to open two physical devices
`/dev/ttyS5` and `/dev/ttyS6`. `/dev/ttyS5` will proxy data two three virtual
devices `/dev/ttyS5.a`, `/dev/ttyS5.b`, and `/dev/ttyS5.c`. Other applications
may open and read and write to these virtual devices as if they are physical
devices. Only one virtual device is allowed to write to the master (physical)
at a time.

## Example

    # Verify physical serial port is writing data
    $ cat /dev/ttyUSB0
    �~��3�0���0���������������������^C

    # Create system config pointing to serial configuration
    $ cat >/etc/serial-proxy/sproxy.ini <<- EOM
    [system]
    serial-configfile = /etc/serial-proxy/serial.ini
    EOM

    # Create configuration file for serial port
    $ cat > /etc/serial-proxy/serial.ini <<- EOM
    [/dev/ttyUSB0]
    virtuals = app1,app2
    baudrate = 9600
    EOM

    # Start serial-proxy
    $ systemctl start serial-proxy
    # Or
    $ sproxyd -c /etc/serial-proxy/sproxy.ini

    # Check existence of virtual serial ports
    $ ls -la /dev/ttyUSB0.*
    lrwxrwxrwx 1 root root 11 /dev/ttyUSB0.app1 -> /dev/pts/25
    lrwxrwxrwx 1 root root 11 /dev/ttyUSB0.app2 -> /dev/pts/26

    # Verify that data is coming through
    $ cat /dev/ttyUSB0.app1
    �~��3�0���0���������������������^C

    $ cat /dev/ttyUSB0.app2
    �~��3�0���0���������������������^C

    # Configure your other apps that previously used /dev/ttyUSB0 with either
    # /dev/ttyUSB0.app1 or /dev/ttyUSB0.app2

## TODO

- Unit testing
- More platform support
- Improve data flow control
