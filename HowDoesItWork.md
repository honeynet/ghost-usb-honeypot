# Technical documentation #

On this page I'll document Ghost from a technical perspective. Unfortunately, most of the contents have yet to be written, so you might want to look at my [presentation of Ghost](http://www.youtube.com/watch?v=9G9oo3b9qR4) in the meantime.

## The goal ##

What we'd like to achieve is the realistic emulation of a USB flash drive. The crucial part is that malware mustn't be able to distinguish between a physical USB flash drive and our emulated Ghost drive. Note that we operate on infected machines, so we have to take extra care to conceal the true nature of the emulated devices.

Whenever a process writes data to the Ghost drive, we assume it to be malicious and collect information about the potential threat.

## High-level overview ##

The main parts of Ghost are implemented as two drivers:
  * GhostBus controls a (virtual) bus device. The device is created at boot-time (a so-called "root-enumerated device") and is present all the time. Its only responsibility is to report Ghost drives to the operating system on demand.
  * Whenever the virtual bus reports a Ghost drive, Windows loads the second driver, GhostDrive. GhostDrive controls the emulated USB flash drive (the "Ghost drive") and strives to behave exactly like `disk.sys`, the operating system's disk class driver. Thus, the Ghost drive appears to be a real removable storage device to anyone looking on it from user space or the higher levels in the kernel-mode storage architecture.

There are different frontends to control Ghost's operation (a command-line tool, a graphical interface), but they work essentially the same way:
  1. The frontend tells the virtual bus device (controlled by GhostBus) to report a new virtual flash drive.
  1. While the flash drive is mounted and visible to all user-space applications, the frontend queries GhostDrive for information on potentially malicious processes.
  1. Eventually, the frontend asks GhostBus to remove the virtual flash drive.

The remainder of this wiki section will explain the concepts in more detail.