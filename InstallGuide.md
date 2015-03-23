# How to install the honeypot #

Starting with version 0.2, Ghost features an installer that does most of the work for you. In a nutshell, all you have to do is get the two dependencies and run `Setup.exe` with administrative privileges. The remainder of this page will guide you through the procedure.

If you have installed a previous version of Ghost on your machine, don't worry: The installer can just as well update the old version. Depending on the version that is currently installed, you might see a warning saying that the old drivers aren't removed. This is not a problem, because the operating system will automatically choose the most recent driver version.

## Install ##

Installing the honeypot consists of three steps:
  1. Install the two dependencies.
  1. Run Ghost's installer.
  1. Prepare the images.

In this guide, we will assume that you downloaded and extracted the binary package of Ghost for your operating system, available from the [downloads section](http://code.google.com/p/ghost-usb-honeypot/downloads/list). Alternatively, you may [build the honeypot from source](BuildGuide.md) before you follow the instructions provided on this page.

**Note:** You shouldn't move the directory with Ghost's files once the system is installed, so place it somewhere convenient **before** you proceed.

### Dependencies ###

Note that you'll need a reasonably recent version of Windows Installer to install either dependency. On most systems, however, a recent version should be installed already.

Ghost needs the Windows Driver Framework (WDF) in any case. If you're running Ghost on Windows 7, the required WDF version 1.9 comes preinstalled, so you're lucky. For Windows XP you can download the WDF files [here](http://msdn.microsoft.com/en-US/windows/hardware/br259104). After you've installed the WDF, there will be a folder `C:\Program files\Windows Kits\8.0\redist\wdf\x86` (if you didn't change the default location) containing among others the two files `WdfCoInstaller01009.dll` and `WUDFUpdate01009.dll`. Copy those two into the extracted Ghost package (they must reside in the same folder as `ghostbus.inf`).

If you'd like to use the graphical frontend, then you also need the .NET framework version 4 or higher. You can download an installer [here](http://www.microsoft.com/en-us/download/details.aspx?id=17718). _This is only required for the graphical frontend._

### Ghost's installer ###

Once you've installed the dependencies, you're ready to install Ghost. Open a command prompt with administrative rights (on Windows 7, right-click and choose "run as administrator") and `cd` into the directory with Ghost's files. Run "Setup.exe" -- that's it.

**Note:** The operating system will most likely ask you whether you really want to install the unsigned drivers that Ghost uses. Basically, what this means is that Microsoft hasn't checked the quality of the driver and that it is not secured against modification.

### Image files ###

The emulated devices that Ghost creates to detect USB malware are backed by image files. They are just binary images of the virtual device's contents, much like the images that you can create with `dd`. Ghost's configuration specifies a location for each device's image. By default, the images for devices 0 to 8 are named `C:\gd[0-8].img`, and the image for device 9 (which the GUI uses) is stored at `C:\gdgui.img`.

The easy way is to download and extract the preformatted clean image file from the [downloads section](http://code.google.com/p/ghost-usb-honeypot/downloads/list) and rename it approrpiately (e.g. `C:\gdgui.img` if you use the graphical frontend or `C:\gd0.img` if you utilize the command line frontend to mount device 0). Alternatively, Ghost will create an empty image file if none is present. Note, however, that you have to format such an empty image before it's usable. On Windows XP, you can use Windows Explorer to do so. We don't currently support formatting emulated devices on Windows 7, so there you'll have to use preformatted image files.

**Note:** When formatting a virtual device, obviously some process will have to write data to it. Usually, it will be Explorer.exe, so don't be alarmed when Ghost reports write access from that process after you've formatted.

## What's next? ##

The honeypot is now ready for use. See the wiki pages on the [command line frontend](Usage.md) and the [graphical frontend](GraphicalFrontend.md) for instructions.