# Enumeration in the Ghost system #

On this page we'll discuss how the Windows detects devices and loads drivers for them, as well as how Ghost uses the mechanisms.

## The concept ##

Enumeration is the process of a device reporting devices that are attached to it.

### Bus enumeration ###

In the Windows driver context, devices that are capable of having child devices are called _bus devices_. Popular examples include the PCI bus and USB hubs and controllers. The Plug and Play Manager, a component of the Windows kernel, regularly sends IRPs with major code IRP\_MJ\_PNP to bus devices in order to query their bus relations (see [Communication](Communication.md) for details on IRPs). The devices are expected to respond with a list of their children and their respective properties.

Based on the properties of the devices a bus driver enumerates, Windows is able to find a driver that suits the new device. Most notably, it considers the so-called _Device ID_, a simple string identifying the type and model of a device.

### Root enumeration ###

Now if every device is enumerated by some other device, where does the chain start? Or in other words, who enumerates the first device? (This is not intended to be a philosophical discussion...)

At this point, root enumeration comes into play. Some devices are just _assumed to be there_ at system startup - they are called _root-enumerated devices_. Most notably, the PCI bus is root-enumerated.

## Usage in Ghost ##

In Ghost, we don't want the emulated USB flash drives to be always present, so we can't root-enumerate them. That's why there is a second type of device in our concept: the virtual bus. The bus device controlled by GhostBus is root-enumerated and thus always present. And how do we enumerate the Ghost drives? You name it, they're bus-enumerated by our virtual bus!