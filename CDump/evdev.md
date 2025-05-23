# Haka
- [ ] Need it to run in the bg and listen to key combinations
@least on linux:

- [*uinput Module*](https://www.kernel.org/doc/html/v4.12/input/uinput.html)
    - kernel module that makes it possible to emulate input devices from userspace.
    - By writing to /dev/uinput (or /dev/input/uinput) device, a process can create a virtual input device with specific capabilities.
    - Once this virtual device is created, the process can send events through it, that will be delivered to userspace and in-kernel consumers.

- [*libevedev*](https://www.freedesktop.org/wiki/Software/libevdev/)
    - *evdev* -> an event device, it generalizes raw input events from device drivers and makes them available through character devices in the /dev/input/ directory [1](https://en.wikipedia.org/wiki/Evdev)
    - libevdev is a wrapper library for evdev devices.
    - It moves the common tasks when dealing with evdev devices into a library and provides a library interface to the callers, thus avoiding erroneous ioctls, etc.
    - *ioctls* -> Input Output Control Codes, used for communication between user-mode applications and drivers, or for communication internally among drivers in a stack. [2](https://learn.microsoft.com/en-us/windows-hardware/drivers/kernel/introduction-to-i-o-control-codes)

- [Monitor Device Events in Linux](https://www.baeldung.com/linux/monitor-device-events)
    - *udev* -> dynamic device manager for Linux Kernel
    - When you connect a keyboard, Linux assigns it an event node (/dev/input/eventX). This is basically a special file that streams input events like key presses, mouse movements, etc. The OS writes raw input data to this file.

- [Mouse and Input Events Interface in Linux](https://www.baeldung.com/linux/mouse-events-input-event-interface)
