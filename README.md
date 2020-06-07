
[![Build Status](https://travis-ci.com/metebalci/novalpmd.svg?branch=master)](https://travis-ci.com/metebalci/novalpmd)

# Novation Launchpad Mini Daemon - UDP Controller

This is a small daemon to control Novation Launchpad Mini with UDP messages.

This program runs under Linux and requires alsa.

The pad keys are addressed from bottom left (1, 1) to top right (8, 8).

# Install

- build the software with a simple `make`

- copy novalpmd.conf to /etc/ and configure the device name and ports

- copy novalpmd.service to ~/.config/systemd/user/ and set the ExecStart value correctly

You can learn the device name with `amidi -l` command.

You can run and control the daemon with `systemctl`, it logs to journal.

# Usage

Pressing the bottom left button (1, 1) gives this output:

```
$ netcat -ul 8888

1 1 127
1 1 0
```

1 1 is the coordinate of the pad key, the third number, 127 and 0 is the velocity. 127 means press, 0 means release.

Controlling the bottom left button (1, 1) color to turn it to red.

```
$ echo -n "0 1 1 5\n" | nc -4u -w0 localhost 8888
```

First number 0 means static color, 2 is pulsing color.
Second and third numbers are the pad key coordinates, 1 1 above.
Last number is the velocity which controls the color. See the official programmers reference manual for color codes. Use color number 0 to disable the color.

You can also use flashing color (first number=1), but then you have to provide 2 colors, for example, to flash between red (5) and blue (41):

```
$ echo -n "1 1 1 5 41\n" | nc -4u -w0 localhost 8888
```

Finally, you can also use RGB colors as long as it is supported by the controller. Set the first number to 3, and provide 3 numbers (R, G, B) for color, for example setting (1, 1) to RGB(255, 255, 0):

```
$ echo -n "3 1 1 255 255 0\n" | nc -4u -w0 localhost 8888
```

Controller supports RGB codes from 0 to 127 (MIDI limitation), so the provided values are divided by 2 internally in the daemon.

# References

- [Launchpad Mini Programmer's Reference Manual](https://fael-downloads-prod.focusrite.com/customer/prod/s3fs-public/downloads/Launchpad%20Mini%20-%20Programmers%20Reference%20Manual.pdf)
