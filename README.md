# PiMSF-baseband - Baseband MSF Time Signal for Raspberry Pi

This is a small program for emulating the MSF Radio Time Signal using a Raspberry Pi. It's forked from marksmanuk/pimsf (which modulates it to a 60kHz carrier).

Written to time synchronise a Wharton digital clock which needs an input pin modulating with MSF signal. The GPIO output (input to the clock) is driven LOW when active.

## Getting Started

Download the latest release of this program and copy the files to your Raspberry Pi.  The code was developed and tested using Raspberry Pi OS Lite on a version Raspberry Pi Zero.

To compile your own version, copy the Makefile and pimsf.c to the Pi.  You will need gcc installed.

### Compiling

```
$ make
```

### Usage

To start the program, enter:

```
$ sudo ./pimsf
```

For help enter:

```
$ sudo ./pimsf -h
Usage: pimsf [options]
        -v Verbose
```

The time signal is sent indefinitely.

### Accuracy

The program sends the system date & time from the Raspberry Pi.  I've assumed the Pi is running NTP and synchronised to a suitable NTP time server over the internet.  In testing I've been using the Chrony replacement NTP daemon.

### Prerequisites

* Raspberry Pi
* gcc - to complile the program

## Authors

* **Mark Street** [marksmanuk](https://github.com/marksmanuk)
* **Nick Piggott** [nickpiggott](https://github.com/nickpiggott)

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details

