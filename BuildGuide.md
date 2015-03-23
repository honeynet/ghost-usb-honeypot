# How to build the honeypot #

**Note:** There is a binary distribution of Ghost, which does not require you to build any code. If you choose to use that version, you can directly proceed to the [install guide](InstallGuide.md).

If you'd like to work with the latest development version of Ghost, you will have to compile the code yourself. This page explains how to do so.

## Prerequisites ##

There are two things that you have to take care of before you can build: a build environment and the source code.

### Build environment ###

You need a build environment that is capable of building Windows drivers. The only one that has been confirmed to be working for this project is the Windows Driver Kit (WDK) from Microsoft. You should use the stable version 7. Microsoft offers it for download on [this page](http://www.microsoft.com/en-us/download/details.aspx?id=11800).

### Source code ###

You can download an archive containing all source code for the release version in the [downloads section](http://code.google.com/p/ghost-usb-honeypot/downloads/list) of this project. If you'd like to use the latest development version instead, you can get its source code by running `git clone https://code.google.com/p/ghost-usb-honeypot/` (provided that git is installed on your machine).

**Note:** Do make sure that the path to your local copy of the source code does not contain spaces! Otherwise, the WDK build tools will fail to build the code.

## Build process ##

**Note:** Two little bugs have slipped into the configuration files of the current version. In `GhostLib\sources`, please change "..\..\GhostDrive" into "..\GhostDrive" (line 4). The second bug requires that you build the code for Windows XP once before you build for Windows 7. They will be fixed in the next version.

It is quite easy to actually build the code. The only thing you have to do is execute the command `build` in the source directory and compile the GUI with a C# compiler. But let's proceed step by step:

  1. Open a build shell. Usually, you can find it in your start menu under _All Programs - Windows Driver Kits - <Your WDK version> - Build Environments_. If you'd like to see debug messages (using tools such as DebugView), select a checked environment, otherwise go for the free one.
  1. In the shell window that pops up, navigate to the folder where you saved Ghost's source code.
  1. Run `build`. This may take a while, because it compiles all source code except the GUI.
  1. If you'd like to build the GUI as well, just open the folder GhostGUI and compile the code with your favorite C# compiler. There's a project file for Visual Studio in the folder with all the relevant settings.
  1. Now you have to collect the build products. Since I develop on a Unix system, I have a bash script to do so. Run `./MakeRelease.sh [winxp|win7] [chk]`. Append `chk` only if you built in the checked environment rather than the free one. The script will create a ZIP file `ghost-win[XP|7].zip` for redistribution and a directory `Release [7|XP]` from which you can directly install the honeypot.

Once you have successfully built all components, you are ready to install Ghost on your machine (see the [install guide](InstallGuide.md)).