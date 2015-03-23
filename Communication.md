# Communication in the Ghost system #

On this page, we'll cover how the various components of Ghost communicate with each other. But let's start by talking about communication in the Windows kernel in general.

## Windows kernel communication ##

The common way for Windows devices to communicate are _I/O Request Packets (IRPs)_. Each IRP contains a major and a minor function code, which determine the purpose of the packet. Examples for major codes are IRP\_MJ\_READ, IRP\_MJ\_WRITE and IRP\_MJ\_POWER. Depending on the type of IRP there are input and/or output buffers associated with it. A Read IRP, for example, needs an output buffer for the driver to store the data read, while a Write IRP always features an input buffer containing the data to write to the device.

Windows uses IRPs extensively to communicate with devices, and it's important to support all IRP function codes that Windows expects to be supported in order to achieve a realistic emulation. The set of function codes to implement depends on the type of device our driver controls: A disk device is expected to be able to read and write data, while this is not a requirement for a bus device.

An important class of IRPs is IRP\_MJ\_DEVICE\_CONTROL. Device Control IRPs are used to, well, control devices - the system uses them to query information from devices (such as the size of a disk) and to influence devices' operation. Also, it is possible to send Device Control IRPs to devices from user space, which is why we use them to control Ghost from the user-mode frontend.

Note that we don't have to handle all IRPs ourselves. Since Ghost uses the _Windows Driver Frameworks (WDF)_, we only need to implement a subset of all possible IRPs. The WDF abstracts many of them or provides sensible default behavior.

Now that we've covered the general communication concept in the kernel, we're ready to talk about Ghost in particular.

## The actors ##

Let's first pin down the components involved:
  * GhostBus, the driver in charge of the virtual bus device
  * GhostDrive, which controls the emulated USB flash drives
  * User-mode frontends
    * GhostLib, a user-mode library that any application may use to control Ghost (e.g. !GhostGUI)
    * GhostTool, a command-line frontend that bypasses GhostLib for historic reasons.

Now let's have a look at the communication between those actors.

### Frontend and GhostBus ###

The frontend uses Device Control IRPs to inform the virtual bus when it's supposed to enumerate a new child device to the system. For that purpose, we use a self-defined subtype of IRP that contains the device ID of the new device in its input buffer. The same holds for requests to remove a virtual device.

### Frontend and GhostDrive ###

While the Ghost drive is mounted, the driver waits for write requests, because they indicate malicious processes. If the emulated flash drive receives a write request, it tries to collect information about the writer (e.g. process and thread ID, a list of loaded modules). The information that it collects needs to be transmitted to the frontend in order to be displayed to the user. However, there is no way for a driver to call user-mode code, which is why we have to work the other way round: The frontend sends Device Control IRPs to the Ghost drive asking for information about malicious processes. Whenever GhostDrive collects such information, it copies the data into the IRPs output buffer and returns it to the frontend.

### GhostBus and GhostDrive ###

At the moment, there is almost no communication between the virtual bus and the emulated flash drives.